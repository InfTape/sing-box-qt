#include "SystemProxy.h"
#include "storage/AppSettings.h"
#include "storage/ConfigConstants.h"
#include "utils/Logger.h"
#ifdef Q_OS_WIN
#include <QSettings>
#include <windows.h>
#include <wininet.h>
#endif
SystemProxy::SystemProxy(QObject* parent) : QObject(parent) {}

bool SystemProxy::setProxy(const QString& host, int port) {
#ifdef Q_OS_WIN
  QString proxyServer = QString("%1:%2").arg(host).arg(port);
  QString bypass      = AppSettings::instance().systemProxyBypass().trimmed();
  if (bypass.isEmpty()) {
    bypass = ConfigConstants::DEFAULT_SYSTEM_PROXY_BYPASS;
  }
  QSettings settings(
      "HKEY_CURRENT_"
      "USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
      QSettings::NativeFormat);
  const bool    alreadyEnabled  = settings.value("ProxyEnable", 0).toInt() == 1;
  const QString currentServer   = settings.value("ProxyServer").toString();
  const QString currentOverride = settings.value("ProxyOverride").toString();
  if (alreadyEnabled && currentServer == proxyServer &&
      currentOverride == bypass) {
    Logger::info(
        QString("System proxy already set to %1, skip refresh").arg(proxyServer));
    return true;
  }
  settings.setValue("ProxyEnable", 1);
  settings.setValue("ProxyServer", proxyServer);
  settings.setValue("ProxyOverride", bypass);
  settings.sync();
  refreshSettings();
  Logger::info(QString("System proxy set: %1").arg(proxyServer));
  return true;
#else
  // TODO(yourdaddy): implement for macOS/Linux.
  Q_UNUSED(host)
  Q_UNUSED(port)
  return false;
#endif
}

bool SystemProxy::clearProxy() {
#ifdef Q_OS_WIN
  if (!isProxyEnabled()) {
    Logger::info("System proxy already disabled, skip refresh");
    return true;
  }
  QSettings settings(
      "HKEY_CURRENT_"
      "USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
      QSettings::NativeFormat);
  settings.setValue("ProxyEnable", 0);
  settings.sync();
  refreshSettings();
  Logger::info("System proxy cleared");
  return true;
#else
  return false;
#endif
}

bool SystemProxy::isProxyEnabled() {
#ifdef Q_OS_WIN
  QSettings settings(
      "HKEY_CURRENT_"
      "USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
      QSettings::NativeFormat);
  return settings.value("ProxyEnable", 0).toInt() == 1;
#else
  return false;
#endif
}

QString SystemProxy::getProxyHost() {
#ifdef Q_OS_WIN
  QSettings settings(
      "HKEY_CURRENT_"
      "USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
      QSettings::NativeFormat);
  QString proxyServer = settings.value("ProxyServer").toString();
  int     colonIndex  = proxyServer.lastIndexOf(':');
  if (colonIndex != -1) {
    return proxyServer.left(colonIndex);
  }
  return proxyServer;
#else
  return QString();
#endif
}

int SystemProxy::getProxyPort() {
#ifdef Q_OS_WIN
  QSettings settings(
      "HKEY_CURRENT_"
      "USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
      QSettings::NativeFormat);
  QString proxyServer = settings.value("ProxyServer").toString();
  int     colonIndex  = proxyServer.lastIndexOf(':');
  if (colonIndex != -1) {
    return proxyServer.mid(colonIndex + 1).toInt();
  }
  return 0;
#else
  return 0;
#endif
}

bool SystemProxy::setPacProxy(const QString& pacUrl) {
#ifdef Q_OS_WIN
  QSettings settings(
      "HKEY_CURRENT_"
      "USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
      QSettings::NativeFormat);
  if (settings.value("AutoConfigURL").toString() == pacUrl) {
    Logger::info(QString("PAC proxy already set to %1, skip refresh").arg(pacUrl));
    return true;
  }
  settings.setValue("AutoConfigURL", pacUrl);
  settings.sync();
  refreshSettings();
  Logger::info(QString("PAC proxy set: %1").arg(pacUrl));
  return true;
#else
  Q_UNUSED(pacUrl)
  return false;
#endif
}

void SystemProxy::refreshSettings() {
#ifdef Q_OS_WIN
  // Notify the system that proxy settings changed.
  InternetSetOptionW(nullptr, INTERNET_OPTION_SETTINGS_CHANGED, nullptr, 0);
  InternetSetOptionW(nullptr, INTERNET_OPTION_REFRESH, nullptr, 0);
#endif
}
