#include "SystemProxy.h"
#include "utils/Logger.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <wininet.h>
#include <QSettings>
#endif

SystemProxy::SystemProxy(QObject *parent)
    : QObject(parent)
{
}

bool SystemProxy::setProxy(const QString &host, int port)
{
#ifdef Q_OS_WIN
    QString proxyServer = QString("%1:%2").arg(host).arg(port);
    
    QSettings settings(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
        QSettings::NativeFormat
    );
    
    settings.setValue("ProxyEnable", 1);
    settings.setValue("ProxyServer", proxyServer);
    settings.setValue("ProxyOverride", "localhost;127.*;10.*;172.16.*;172.17.*;172.18.*;172.19.*;172.20.*;172.21.*;172.22.*;172.23.*;172.24.*;172.25.*;172.26.*;172.27.*;172.28.*;172.29.*;172.30.*;172.31.*;192.168.*;<local>");
    settings.sync();
    
    refreshSettings();
    
    Logger::info(QString("系统代理已设置: %1").arg(proxyServer));
    return true;
#else
    // TODO: macOS/Linux 实现
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
    
    Logger::info("系统代理已清除");
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
    
    Logger::info(QString("PAC 代理已设置: %1").arg(pacUrl));
    return true;
#else
    Q_UNUSED(pacUrl)
    return false;
#endif
}

void SystemProxy::refreshSettings()
{
#ifdef Q_OS_WIN
    // 通知系统代理设置已更改
    InternetSetOptionW(nullptr, INTERNET_OPTION_SETTINGS_CHANGED, nullptr, 0);
    InternetSetOptionW(nullptr, INTERNET_OPTION_REFRESH, nullptr, 0);
#endif
}
