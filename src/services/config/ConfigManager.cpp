#include "services/config/ConfigManager.h"

#include "services/config/ConfigBuilder.h"
#include "services/config/ConfigMutator.h"
#include "storage/AppSettings.h"
#include "storage/ConfigIO.h"
ConfigManager& ConfigManager::instance() {
  static ConfigManager instance;
  return instance;
}
ConfigManager::ConfigManager(QObject* parent) : QObject(parent) {}
QString ConfigManager::getConfigDir() const { return ConfigIO::getConfigDir(); }
QString ConfigManager::getActiveConfigPath() const {
  return ConfigIO::getActiveConfigPath();
}
QJsonObject ConfigManager::generateBaseConfig() {
  QJsonObject config = ConfigBuilder::buildBaseConfig();
  ConfigMutator::applySettings(config);
  return config;
}
bool ConfigManager::generateConfigWithNodes(const QJsonArray& nodes,
                                            const QString&    targetPath) {
  QJsonObject config = generateBaseConfig();
  if (!ConfigMutator::injectNodes(config, nodes)) {
    return false;
  }

  const QString path =
      targetPath.isEmpty() ? getActiveConfigPath() : targetPath;
  return ConfigIO::saveConfig(path, config);
}
bool ConfigManager::injectNodes(QJsonObject& config, const QJsonArray& nodes) {
  return ConfigMutator::injectNodes(config, nodes);
}
void ConfigManager::applySettingsToConfig(QJsonObject& config) {
  ConfigMutator::applySettings(config);
}
void ConfigManager::applyPortSettings(QJsonObject& config) {
  ConfigMutator::applyPortSettings(config);
}
QJsonObject ConfigManager::loadConfig(const QString& path) {
  return ConfigIO::loadConfig(path);
}
bool ConfigManager::saveConfig(const QString& path, const QJsonObject& config) {
  return ConfigIO::saveConfig(path, config);
}
int ConfigManager::getMixedPort() const {
  return AppSettings::instance().mixedPort();
}
int ConfigManager::getApiPort() const {
  return AppSettings::instance().apiPort();
}
void ConfigManager::setMixedPort(int port) {
  AppSettings::instance().setMixedPort(port);
}
void ConfigManager::setApiPort(int port) {
  AppSettings::instance().setApiPort(port);
}
bool ConfigManager::updateClashDefaultMode(const QString& configPath,
                                           const QString& mode,
                                           QString*       error) {
  QJsonObject config = ConfigIO::loadConfig(configPath);
  if (config.isEmpty()) {
    if (error)
      *error = QString("Failed to load config file: %1").arg(configPath);
    return false;
  }

  if (!ConfigMutator::updateClashDefaultMode(config, mode, error)) {
    return false;
  }

  if (!ConfigIO::saveConfig(configPath, config)) {
    if (error)
      *error = QString("Failed to save config file: %1").arg(configPath);
    return false;
  }

  return true;
}
QString ConfigManager::readClashDefaultMode(const QString& configPath) const {
  const QJsonObject config = ConfigIO::loadConfig(configPath);
  return ConfigMutator::readClashDefaultMode(config);
}
