#include "SystemProxy.h"
#include "storage/AppSettings.h"
#include "storage/ConfigConstants.h"
#include "utils/Logger.h"
#include <QProcess>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wininet.h>
#include <QSettings>
#endif

#ifdef Q_OS_FREEBSD
namespace {
QString gsettingsPath()
{
    return QStandardPaths::findExecutable("gsettings");
}

QString stripGvariantString(const QString &value)
{
    QString trimmed = value.trimmed();
    if (trimmed.startsWith('\'') && trimmed.endsWith('\'')) {
        return trimmed.mid(1, trimmed.size() - 2);
    }
    if (trimmed.startsWith('\"') && trimmed.endsWith('\"')) {
        return trimmed.mid(1, trimmed.size() - 2);
    }
    return trimmed;
}

bool runGsettings(const QStringList &args, QString *errorMessage)
{
    const QString path = gsettingsPath();
    if (path.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "gsettings not found";
        }
        return false;
    }

    QProcess proc;
    proc.start(path, args);
    if (!proc.waitForFinished(5000)) {
        proc.kill();
        if (errorMessage) {
            *errorMessage = "gsettings timed out";
        }
        return false;
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            if (errorMessage->isEmpty()) {
                *errorMessage = "gsettings failed";
            }
        }
        return false;
    }

    return true;
}

QString readGsettings(const QStringList &args, QString *errorMessage)
{
    const QString path = gsettingsPath();
    if (path.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "gsettings not found";
        }
        return QString();
    }

    QProcess proc;
    proc.start(path, args);
    if (!proc.waitForFinished(5000)) {
        proc.kill();
        if (errorMessage) {
            *errorMessage = "gsettings timed out";
        }
        return QString();
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            if (errorMessage->isEmpty()) {
                *errorMessage = "gsettings failed";
            }
        }
        return QString();
    }

    return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
}

bool gsettingsSet(const QString &schema, const QString &key, const QString &value, QString *errorMessage)
{
    return runGsettings({"set", schema, key, value}, errorMessage);
}

QString gsettingsGet(const QString &schema, const QString &key, QString *errorMessage)
{
    return readGsettings({"get", schema, key}, errorMessage);
}
} // namespace
#endif

SystemProxy::SystemProxy(QObject *parent)
    : QObject(parent)
{
}

bool SystemProxy::setProxy(const QString &host, int port)
{
#ifdef Q_OS_WIN
    QString proxyServer = QString("%1:%2").arg(host).arg(port);
    QString bypass = AppSettings::instance().systemProxyBypass().trimmed();
    if (bypass.isEmpty()) {
        bypass = ConfigConstants::DEFAULT_SYSTEM_PROXY_BYPASS;
    }

    QSettings settings(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
        QSettings::NativeFormat
    );

    settings.setValue("ProxyEnable", 1);
    settings.setValue("ProxyServer", proxyServer);
    settings.setValue("ProxyOverride", bypass);
    settings.sync();

    refreshSettings();

    Logger::info(QString("System proxy set: %1").arg(proxyServer));
    return true;
#elif defined(Q_OS_FREEBSD)
    QString errorMessage;
    bool ok = true;

    ok = ok && gsettingsSet("org.gnome.system.proxy", "mode", "'manual'", &errorMessage);
    ok = ok && gsettingsSet("org.gnome.system.proxy", "use-same-proxy", "true", &errorMessage);
    ok = ok && gsettingsSet("org.gnome.system.proxy.http", "host", QString("'%1'").arg(host), &errorMessage);
    ok = ok && gsettingsSet("org.gnome.system.proxy.http", "port", QString::number(port), &errorMessage);
    ok = ok && gsettingsSet("org.gnome.system.proxy.https", "host", QString("'%1'").arg(host), &errorMessage);
    ok = ok && gsettingsSet("org.gnome.system.proxy.https", "port", QString::number(port), &errorMessage);
    ok = ok && gsettingsSet("org.gnome.system.proxy.socks", "host", QString("'%1'").arg(host), &errorMessage);
    ok = ok && gsettingsSet("org.gnome.system.proxy.socks", "port", QString::number(port), &errorMessage);
    ok = ok && gsettingsSet("org.gnome.system.proxy", "ignore-hosts",
                             "['localhost', '127.0.0.0/8', '::1']", &errorMessage);

    if (!ok) {
        Logger::error(QString("Failed to set system proxy: %1").arg(errorMessage));
        return false;
    }

    Logger::info(QString("System proxy set: %1:%2").arg(host).arg(port));
    return true;
#else
    // TODO: implement for macOS/Linux.
    Q_UNUSED(host)
    Q_UNUSED(port)
    return false;
#endif
}

bool SystemProxy::clearProxy()
{
#ifdef Q_OS_WIN
    QSettings settings(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
        QSettings::NativeFormat
    );

    settings.setValue("ProxyEnable", 0);
    settings.sync();

    refreshSettings();

    Logger::info("System proxy cleared");
    return true;
#elif defined(Q_OS_FREEBSD)
    QString errorMessage;
    bool ok = gsettingsSet("org.gnome.system.proxy", "mode", "'none'", &errorMessage);
    ok = ok && gsettingsSet("org.gnome.system.proxy", "autoconfig-url", "''", &errorMessage);
    if (!ok) {
        Logger::error(QString("Failed to clear system proxy: %1").arg(errorMessage));
        return false;
    }
    Logger::info("System proxy cleared");
    return true;
#else
    return false;
#endif
}

bool SystemProxy::isProxyEnabled()
{
#ifdef Q_OS_WIN
    QSettings settings(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
        QSettings::NativeFormat
    );

    return settings.value("ProxyEnable", 0).toInt() == 1;
#elif defined(Q_OS_FREEBSD)
    QString errorMessage;
    const QString mode = stripGvariantString(gsettingsGet("org.gnome.system.proxy", "mode", &errorMessage));
    if (mode.isEmpty() && !errorMessage.isEmpty()) {
        Logger::warn(QString("Failed to read system proxy mode: %1").arg(errorMessage));
    }
    return !mode.isEmpty() && mode != "none";
#else
    return false;
#endif
}

QString SystemProxy::getProxyHost()
{
#ifdef Q_OS_WIN
    QSettings settings(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
        QSettings::NativeFormat
    );

    QString proxyServer = settings.value("ProxyServer").toString();
    int colonIndex = proxyServer.lastIndexOf(':');
    if (colonIndex != -1) {
        return proxyServer.left(colonIndex);
    }
    return proxyServer;
#elif defined(Q_OS_FREEBSD)
    QString errorMessage;
    const QString host = gsettingsGet("org.gnome.system.proxy.http", "host", &errorMessage);
    return stripGvariantString(host);
#else
    return QString();
#endif
}

int SystemProxy::getProxyPort()
{
#ifdef Q_OS_WIN
    QSettings settings(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
        QSettings::NativeFormat
    );

    QString proxyServer = settings.value("ProxyServer").toString();
    int colonIndex = proxyServer.lastIndexOf(':');
    if (colonIndex != -1) {
        return proxyServer.mid(colonIndex + 1).toInt();
    }
    return 0;
#elif defined(Q_OS_FREEBSD)
    QString errorMessage;
    const QString port = gsettingsGet("org.gnome.system.proxy.http", "port", &errorMessage);
    bool ok = false;
    const int value = port.trimmed().toInt(&ok);
    return ok ? value : 0;
#else
    return 0;
#endif
}

bool SystemProxy::setPacProxy(const QString &pacUrl)
{
#ifdef Q_OS_WIN
    QSettings settings(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
        QSettings::NativeFormat
    );

    settings.setValue("AutoConfigURL", pacUrl);
    settings.sync();

    refreshSettings();

    Logger::info(QString("PAC proxy set: %1").arg(pacUrl));
    return true;
#elif defined(Q_OS_FREEBSD)
    QString errorMessage;
    bool ok = gsettingsSet("org.gnome.system.proxy", "autoconfig-url", QString("'%1'").arg(pacUrl), &errorMessage);
    ok = ok && gsettingsSet("org.gnome.system.proxy", "mode", "'auto'", &errorMessage);
    if (!ok) {
        Logger::error(QString("Failed to set PAC proxy: %1").arg(errorMessage));
        return false;
    }
    Logger::info(QString("PAC proxy set: %1").arg(pacUrl));
    return true;
#else
    Q_UNUSED(pacUrl)
    return false;
#endif
}

void SystemProxy::refreshSettings()
{
#ifdef Q_OS_WIN
    // Notify the system that proxy settings changed.
    InternetSetOptionW(nullptr, INTERNET_OPTION_SETTINGS_CHANGED, nullptr, 0);
    InternetSetOptionW(nullptr, INTERNET_OPTION_REFRESH, nullptr, 0);
#endif
}
