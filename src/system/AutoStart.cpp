#include "AutoStart.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include "utils/Logger.h"

namespace {
#ifdef Q_OS_LINUX
QString desktopEntryPath() {
  const QString overridePath =
      qEnvironmentVariable("SING_BOX_QT_AUTOSTART_FILE").trimmed();
  if (!overridePath.isEmpty()) {
    return QDir::cleanPath(overridePath);
  }
  const QString configRoot =
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
  return QDir(configRoot).filePath("autostart/sing-box-qt.desktop");
}

QString escapeDesktopExecArgument(const QString& argument) {
  QString escaped = argument;
  escaped.replace('\\', QStringLiteral("\\\\"));
  escaped.replace('"', QStringLiteral("\\\""));
  escaped.replace('`', QStringLiteral("\\`"));
  escaped.replace('$', QStringLiteral("\\$"));
  return QStringLiteral("\"") + escaped + QStringLiteral("\"");
}
#endif
}  // namespace

bool AutoStart::isSupported() {
#ifdef Q_OS_WIN
  return true;
#elif defined(Q_OS_LINUX)
  return true;
#else
  return false;
#endif
}

bool AutoStart::isEnabled(const QString& appName) {
#ifdef Q_OS_WIN
  QSettings settings(
      "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
      QSettings::NativeFormat);
  const QString name =
      appName.isEmpty() ? QCoreApplication::applicationName() : appName;
  const QString value = settings.value(name).toString();
  if (value.isEmpty()) {
    return false;
  }
  const QString appPath =
      QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
  return value.contains(appPath, Qt::CaseInsensitive);
#elif defined(Q_OS_LINUX)
  Q_UNUSED(appName)
  QFile file(desktopEntryPath());
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }
  const QString content = QString::fromUtf8(file.readAll());
  const QString appPath = QCoreApplication::applicationFilePath();
  return content.contains("Type=Application") &&
         content.contains("Exec=" + escapeDesktopExecArgument(appPath)) &&
         !content.contains("Hidden=true", Qt::CaseInsensitive);
#else
  Q_UNUSED(appName)
  return false;
#endif
}

bool AutoStart::setEnabled(bool enabled, const QString& appName) {
#ifdef Q_OS_WIN
  QSettings settings(
      "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
      QSettings::NativeFormat);
  const QString name =
      appName.isEmpty() ? QCoreApplication::applicationName() : appName;
  if (enabled) {
    const QString appPath =
        QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString value = QString("\"%1\" --hide").arg(appPath);
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
#elif defined(Q_OS_LINUX)
  const QString path = desktopEntryPath();
  if (!enabled) {
    return !QFile::exists(path) || QFile::remove(path);
  }
  const QString dirPath = QFileInfo(path).absolutePath();
  if (!QDir().mkpath(dirPath)) {
    Logger::error(QString("Failed to create auto-start directory: %1")
                      .arg(dirPath));
    return false;
  }
  const QString name =
      appName.isEmpty() ? QCoreApplication::applicationName() : appName;
  const QString appPath = QCoreApplication::applicationFilePath();
  const QString content =
      QStringLiteral("[Desktop Entry]\n"
                     "Type=Application\n"
                     "Version=1.0\n"
                     "Name=%1\n"
                     "Comment=Start Sing-Box Qt in the system tray\n"
                     "Exec=%2 --hide\n"
                     "TryExec=%3\n"
                     "Icon=sing-box-qt\n"
                     "Terminal=false\n"
                     "X-KDE-autostart-after=panel\n"
                     "X-GNOME-Autostart-enabled=true\n")
          .arg(name,
               escapeDesktopExecArgument(appPath),
               escapeDesktopExecArgument(appPath));
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text) ||
      file.write(content.toUtf8()) < 0 || !file.commit()) {
    Logger::error(QString("Failed to write auto-start entry: %1").arg(path));
    return false;
  }
  return true;
#else
  Q_UNUSED(enabled)
  Q_UNUSED(appName)
  return false;
#endif
}
