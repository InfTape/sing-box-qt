#include "SystemProxy.h"
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include "storage/AppSettings.h"
#include "storage/ConfigConstants.h"
#include "utils/Logger.h"
#ifdef Q_OS_WIN
#include <QSettings>
#include <windows.h>
#include <wininet.h>
#endif

namespace {
#ifdef Q_OS_LINUX
constexpr auto kProxyGroup = "Proxy Settings";

enum class KConfigValueType { String, Bool };

QString kdeProxyConfigPath() {
  const QString overridePath =
      qEnvironmentVariable("SING_BOX_QT_KDE_PROXY_CONFIG").trimmed();
  if (!overridePath.isEmpty()) {
    return QDir::cleanPath(overridePath);
  }
  const QString configRoot =
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
  return QDir(configRoot).filePath("kioslaverc");
}

bool isKdeSession() {
  if (qEnvironmentVariableIsSet("SING_BOX_QT_KDE_PROXY_CONFIG")) {
    return true;
  }
  const QString desktop =
      (qEnvironmentVariable("XDG_CURRENT_DESKTOP") + ";" +
       qEnvironmentVariable("XDG_SESSION_DESKTOP"))
          .toLower();
  return desktop.contains("kde") || desktop.contains("plasma");
}

QString kdeBypassList() {
  QString bypass = AppSettings::instance().systemProxyBypass().trimmed();
  if (bypass.isEmpty()) {
    bypass = ConfigConstants::DEFAULT_SYSTEM_PROXY_BYPASS;
  }
  bypass.replace(';', ',');
  bypass.replace(QStringLiteral("<local>"), QStringLiteral("localhost"),
                 Qt::CaseInsensitive);
  return bypass;
}

QString findKConfigTool(const QString& name) {
  return QStandardPaths::findExecutable(
      name, {"/usr/bin", "/usr/local/bin", "/bin"});
}

bool runKWriteConfig(const QString& key,
                     const QString& value,
                     KConfigValueType type = KConfigValueType::String,
                     bool             remove = false,
                     bool             notify = false) {
  if (!isKdeSession()) {
    Logger::warn("System proxy is currently supported on KDE Plasma only");
    return false;
  }
  const QString path = kdeProxyConfigPath();
  if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
    Logger::error(QString("Failed to create KDE config directory: %1")
                      .arg(QFileInfo(path).absolutePath()));
    return false;
  }
  const QString kwriteconfig = findKConfigTool("kwriteconfig6");
  if (kwriteconfig.isEmpty()) {
    Logger::error("kwriteconfig6 is required to update KDE proxy settings");
    return false;
  }
  QStringList args{"--file",
                   path,
                   "--group",
                   QString::fromLatin1(kProxyGroup),
                   "--key",
                   key};
  if (type == KConfigValueType::Bool) {
    args << "--type" << "bool";
  }
  if (remove) {
    args << "--delete";
  }
  if (notify) {
    args << "--notify";
  }
  args << value;

  QProcess process;
  process.start(kwriteconfig, args);
  if (!process.waitForFinished(5000) ||
      process.exitStatus() != QProcess::NormalExit ||
      process.exitCode() != 0) {
    const QString error =
        QString::fromUtf8(process.readAllStandardError()).trimmed();
    Logger::error(error.isEmpty()
                      ? QString("Failed to update KDE proxy key: %1").arg(key)
                      : error);
    return false;
  }
  return true;
}

QString kdeProxyValue(const QString& key) {
  if (!isKdeSession()) {
    return QString();
  }
  const QString kreadconfig = findKConfigTool("kreadconfig6");
  if (kreadconfig.isEmpty()) {
    return QString();
  }
  QProcess process;
  process.start(kreadconfig,
                {"--file",
                 kdeProxyConfigPath(),
                 "--group",
                 QString::fromLatin1(kProxyGroup),
                 "--key",
                 key});
  if (!process.waitForFinished(5000) ||
      process.exitStatus() != QProcess::NormalExit ||
      process.exitCode() != 0) {
    return QString();
  }
  return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}
#endif
}  // namespace

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
  settings.setValue("ProxyEnable", 1);
  settings.setValue("ProxyServer", proxyServer);
  settings.setValue("ProxyOverride", bypass);
  settings.sync();
  refreshSettings();
  Logger::info(QString("System proxy set: %1").arg(proxyServer));
  return true;
#elif defined(Q_OS_LINUX)
  if (host.trimmed().isEmpty() || port <= 0 || port > 65535) {
    return false;
  }
  const QString proxyUrl = QString("http://%1:%2").arg(host).arg(port);
  const QString socksUrl = QString("socks://%1:%2").arg(host).arg(port);
  const bool ok =
      runKWriteConfig("SingBoxQtManaged", "true", KConfigValueType::Bool) &&
      runKWriteConfig("ReversedException", "false",
                      KConfigValueType::Bool) &&
      runKWriteConfig("httpProxy", proxyUrl) &&
      runKWriteConfig("httpsProxy", proxyUrl) &&
      runKWriteConfig("ftpProxy", proxyUrl) &&
      runKWriteConfig("socksProxy", socksUrl) &&
      runKWriteConfig("NoProxyFor", kdeBypassList()) &&
      runKWriteConfig("Proxy Config Script", QString(),
                      KConfigValueType::String, true) &&
      runKWriteConfig("ProxyType", "1", KConfigValueType::String, false,
                      true);
  if (ok) {
    refreshSettings();
    Logger::info(QString("KDE system proxy set: %1").arg(proxyUrl));
  }
  return ok;
#else
  Q_UNUSED(host)
  Q_UNUSED(port)
  return false;
#endif
}

bool SystemProxy::clearProxy() {
#ifdef Q_OS_WIN
  QSettings settings(
      "HKEY_CURRENT_"
      "USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
      QSettings::NativeFormat);
  settings.setValue("ProxyEnable", 0);
  settings.sync();
  refreshSettings();
  Logger::info("System proxy cleared");
  return true;
#elif defined(Q_OS_LINUX)
  if (!isKdeSession()) {
    return false;
  }
  const QString managedValue = kdeProxyValue("SingBoxQtManaged").toLower();
  const bool    managed = managedValue == "true" || managedValue == "1";
  if (!managed) {
    return true;
  }
  bool ok = runKWriteConfig("ProxyType", "0");
  const QStringList keys{"httpProxy",
                         "httpsProxy",
                         "ftpProxy",
                         "socksProxy",
                         "NoProxyFor",
                         "ReversedException",
                         "Proxy Config Script",
                         "SingBoxQtManaged"};
  for (const QString& key : keys) {
    ok = runKWriteConfig(key, QString(), KConfigValueType::String, true) && ok;
  }
  ok = runKWriteConfig("ProxyType", "0", KConfigValueType::String, false,
                       true) &&
       ok;
  if (ok) {
    refreshSettings();
    Logger::info("KDE system proxy cleared");
  }
  return ok;
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
#elif defined(Q_OS_LINUX)
  if (!isKdeSession()) {
    return false;
  }
  return kdeProxyValue("ProxyType").toInt() == 1;
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
#elif defined(Q_OS_LINUX)
  if (!isKdeSession()) {
    return QString();
  }
  return QUrl(kdeProxyValue("httpProxy")).host();
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
#elif defined(Q_OS_LINUX)
  if (!isKdeSession()) {
    return 0;
  }
  return QUrl(kdeProxyValue("httpProxy")).port();
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
  settings.setValue("AutoConfigURL", pacUrl);
  settings.sync();
  refreshSettings();
  Logger::info(QString("PAC proxy set: %1").arg(pacUrl));
  return true;
#elif defined(Q_OS_LINUX)
  const QUrl url(pacUrl);
  if (!url.isValid() || url.scheme().isEmpty()) {
    return false;
  }
  bool ok =
      runKWriteConfig("SingBoxQtManaged", "true", KConfigValueType::Bool) &&
      runKWriteConfig("Proxy Config Script", pacUrl);
  const QStringList keys{"httpProxy", "httpsProxy", "ftpProxy", "socksProxy"};
  for (const QString& key : keys) {
    ok = runKWriteConfig(key, QString(), KConfigValueType::String, true) && ok;
  }
  if (ok) {
    ok = runKWriteConfig("ProxyType", "2", KConfigValueType::String, false,
                         true);
  }
  if (ok) {
    refreshSettings();
  }
  return ok;
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
#elif defined(Q_OS_LINUX)
  const QString dbusSend = QStandardPaths::findExecutable("dbus-send");
  if (dbusSend.isEmpty()) {
    return;
  }
  QProcess process;
  process.start(dbusSend,
                {"--session",
                 "--type=signal",
                 "/KIO/Scheduler",
                 "org.kde.KIO.Scheduler.reparseSlaveConfiguration",
                 "string:"});
  process.waitForFinished(1000);
#endif
}
