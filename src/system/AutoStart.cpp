#include "AutoStart.h"
#include "utils/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

namespace {
QString resolveAppName(const QString &appName)
{
    QString name = appName.isEmpty() ? QCoreApplication::applicationName() : appName;
    if (name.isEmpty()) {
        name = "sing-box-qt";
    }
    return name;
}

QString autostartDir()
{
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (configDir.isEmpty()) {
        return QString();
    }
    return QDir(configDir).filePath("autostart");
}

QString autostartFilePath(const QString &appName)
{
    const QString dir = autostartDir();
    if (dir.isEmpty()) {
        return QString();
    }
    QString name = resolveAppName(appName);
    name.replace(' ', '-');
    return QDir(dir).filePath(name + ".desktop");
}

QString buildExecLine()
{
    const QString appPath = QCoreApplication::applicationFilePath();
    return QString("\"%1\" --hide").arg(appPath);
}

bool writeDesktopFile(const QString &filePath, const QString &appName)
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out << "[Desktop Entry]\n";
    out << "Type=Application\n";
    out << "Name=" << resolveAppName(appName) << "\n";
    out << "Exec=" << buildExecLine() << "\n";
    out << "X-GNOME-Autostart-enabled=true\n";
    out << "NoDisplay=false\n";
    return file.commit();
}

QString readDesktopExec(const QString &filePath)
{
    QSettings settings(filePath, QSettings::IniFormat);
    settings.beginGroup("Desktop Entry");
    return settings.value("Exec").toString();
}
} // namespace

bool AutoStart::isSupported()
{
#ifdef Q_OS_WIN
    return true;
#else
    return !autostartDir().isEmpty();
#endif
}

bool AutoStart::isEnabled(const QString &appName)
{
#ifdef Q_OS_WIN
    QSettings settings(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        QSettings::NativeFormat
    );
    const QString name = appName.isEmpty()
        ? QCoreApplication::applicationName()
        : appName;
    const QString value = settings.value(name).toString();
    if (value.isEmpty()) {
        return false;
    }
    const QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    return value.contains(appPath, Qt::CaseInsensitive);
#else
    const QString filePath = autostartFilePath(appName);
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        return false;
    }
    const QString exec = readDesktopExec(filePath);
    if (exec.isEmpty()) {
        return false;
    }
    const QString appPath = QCoreApplication::applicationFilePath();
    return exec.contains(appPath);
#endif
}

bool AutoStart::setEnabled(bool enabled, const QString &appName)
{
#ifdef Q_OS_WIN
    QSettings settings(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        QSettings::NativeFormat
    );
    const QString name = appName.isEmpty()
        ? QCoreApplication::applicationName()
        : appName;

    if (enabled) {
        const QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        const QString value = QString("\"%1\"").arg(appPath);
        settings.setValue(name, value);
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            Logger::error("Failed to write auto-start registry entry");
            return false;
        }
        return true;
    }

    if (settings.contains(name)) {
        settings.remove(name);
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            Logger::error("Failed to remove auto-start registry entry");
            return false;
        }
    }
    return true;
#else
    const QString dir = autostartDir();
    const QString filePath = autostartFilePath(appName);
    if (dir.isEmpty() || filePath.isEmpty()) {
        Logger::error("Auto-start config directory not available");
        return false;
    }

    if (enabled) {
        QDir().mkpath(dir);
        if (!writeDesktopFile(filePath, appName)) {
            Logger::error("Failed to write auto-start desktop entry");
            return false;
        }
        return true;
    }

    if (QFile::exists(filePath) && !QFile::remove(filePath)) {
        Logger::error("Failed to remove auto-start desktop entry");
        return false;
    }
    return true;
#endif
}
