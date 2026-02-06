#include "ProxyController.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include "KernelService.h"
#include "app/interfaces/ConfigRepository.h"
#include "app/interfaces/SettingsStore.h"
#include "app/interfaces/SystemProxyGateway.h"
#include "network/SubscriptionService.h"
#include "utils/Logger.h"
ProxyController::ProxyController(KernelService* kernel, SubscriptionService* subscription, ConfigRepository* configRepo,
                                 SettingsStore* settings, SystemProxyGateway* systemProxy, QObject* parent)
    : QObject(parent),
      m_kernel(kernel),
      m_subscription(subscription),
      m_configRepo(configRepo),
      m_settings(settings),
      m_systemProxy(systemProxy) {
  Q_ASSERT(m_configRepo && "ProxyController requires ConfigRepository");
  Q_ASSERT(m_settings && "ProxyController requires SettingsStore");
  Q_ASSERT(m_systemProxy && "ProxyController requires SystemProxyGateway");
}
void ProxyController::setSubscriptionService(SubscriptionService* service) {
  m_subscription = service;
}
QString ProxyController::activeConfigPath() const {
  QString configPath;
  if (m_subscription) {
    configPath = m_subscription->getActiveConfigPath();
  }
  if (configPath.isEmpty() && m_configRepo) {
    configPath = m_configRepo->getActiveConfigPath();
  }
  return configPath;
}
QString ProxyController::currentProxyMode() const {
  const QString path = activeConfigPath();
  if (path.isEmpty() || !m_configRepo) return "rule";
  return m_configRepo->readClashDefaultMode(path);
}
bool ProxyController::ensureConfigExists(QString* outPath) {
  QString configPath = activeConfigPath();
  if (!configPath.isEmpty() && QFile::exists(configPath)) {
    if (outPath) *outPath = configPath;
    return true;
  }
  if (!m_configRepo) {
    Logger::error(QStringLiteral("Config repository not available"));
    return false;
  }
  if (!m_configRepo->generateConfigWithNodes(QJsonArray())) {
    Logger::error(QStringLiteral("Failed to generate default config"));
    return false;
  }
  configPath = m_configRepo->getActiveConfigPath();
  if (outPath) *outPath = configPath;
  return QFile::exists(configPath);
}
bool ProxyController::startKernel() {
  if (!m_kernel) return false;
  QString configPath;
  if (!ensureConfigExists(&configPath)) {
    return false;
  }
  m_kernel->setConfigPath(configPath);
  return m_kernel->start(configPath);
}
void ProxyController::stopKernel() {
  if (m_kernel) {
    m_kernel->stop();
  }
}
bool ProxyController::toggleKernel() {
  if (!m_kernel) return false;
  if (m_kernel->isRunning()) {
    m_kernel->stop();
    return true;
  }
  return startKernel();
}
bool ProxyController::setProxyMode(const QString& mode, bool restartIfRunning, QString* error) {
  const QString configPath = activeConfigPath();
  if (configPath.isEmpty()) {
    if (error) *error = QObject::tr("Failed to resolve config path");
    return false;
  }
  if (!m_configRepo) {
    if (error) *error = QObject::tr("Config repository not available");
    return false;
  }
  bool ok = m_configRepo->updateClashDefaultMode(configPath, mode, error);
  if (ok && restartIfRunning && m_kernel && m_kernel->isRunning()) {
    m_kernel->restartWithConfig(configPath);
  }
  return ok;
}
bool ProxyController::restartKernelWithConfig(const QString& configPath) {
  if (!m_kernel || configPath.isEmpty()) return false;
  m_kernel->setConfigPath(configPath);
  if (m_kernel->isRunning()) {
    m_kernel->restartWithConfig(configPath);
  } else {
    m_kernel->start(configPath);
  }
  return true;
}
bool ProxyController::setSystemProxyEnabled(bool enabled) {
  if (enabled) {
    const int port = m_configRepo ? m_configRepo->mixedPort() : 1080;
    m_systemProxy->setProxy("127.0.0.1", port);
    m_settings->setSystemProxyEnabled(true);
  } else {
    m_systemProxy->clearProxy();
    m_settings->setSystemProxyEnabled(false);
  }
  // System proxy toggle should not force a kernel restart.
  return applySettingsToActiveConfig(false);
}
bool ProxyController::setTunModeEnabled(bool enabled) {
  m_settings->setTunEnabled(enabled);
  return applySettingsToActiveConfig(true);
}
bool ProxyController::applySettingsToActiveConfig(bool restartIfRunning) {
  QString configPath = activeConfigPath();
  if (configPath.isEmpty() || !m_configRepo) return false;
  QJsonObject config = m_configRepo->loadConfig(configPath);
  if (config.isEmpty()) return false;
  m_configRepo->applySettingsToConfig(config);
  m_configRepo->saveConfig(configPath, config);
  if (restartIfRunning && m_kernel && m_kernel->isRunning()) {
    m_kernel->restartWithConfig(configPath);
  }
  return true;
}
void ProxyController::updateSystemProxyForKernelState(bool running) {
  const bool systemProxyEnabled = m_settings->systemProxyEnabled();
  if (!systemProxyEnabled) {
    m_systemProxy->clearProxy();
    return;
  }
  if (running) {
    const int port = m_configRepo ? m_configRepo->mixedPort() : 1080;
    m_systemProxy->setProxy("127.0.0.1", port);
  } else {
    m_systemProxy->clearProxy();
  }
}
