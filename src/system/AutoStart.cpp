#include "AutoStart.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

#include "utils/Logger.h"
bool AutoStart::isSupported() {
#ifdef Q_OS_WIN
  return true;
#else
  return false;
#endif
}
bool AutoStart::isEnabled(const QString& appName) {
#ifdef Q_OS_WIN
  QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
  const QString name  = appName.isEmpty() ? QCoreApplication::applicationName() : appName;
  const QString value = settings.value(name).toString();
  if (value.isEmpty()) {
    return false;
  }
  const QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
  return value.contains(appPath, Qt::CaseInsensitive);
#else
  Q_UNUSED(appName)
  return false;
#endif
}
bool AutoStart::setEnabled(bool enabled, const QString& appName) {
#ifdef Q_OS_WIN
  QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
  const QString name = appName.isEmpty() ? QCoreApplication::applicationName() : appName;

  if (enabled) {
    const QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString value   = QString("\"%1\"").arg(appPath);
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
  Q_UNUSED(enabled)
  Q_UNUSED(appName)
  return false;
#endif
}
