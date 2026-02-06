#include "ConfigRepositoryAdapter.h"
#include "services/config/ConfigManager.h"

QString ConfigRepositoryAdapter::getConfigDir() const {
  return ConfigManager::instance().getConfigDir();
}

QString ConfigRepositoryAdapter::getActiveConfigPath() const {
  return ConfigManager::instance().getActiveConfigPath();
}

bool ConfigRepositoryAdapter::generateConfigWithNodes(const QJsonArray& nodes, const QString& targetPath) {
  return ConfigManager::instance().generateConfigWithNodes(nodes, targetPath);
}

bool ConfigRepositoryAdapter::updateClashDefaultMode(const QString& configPath, const QString& mode, QString* error) {
  return ConfigManager::instance().updateClashDefaultMode(configPath, mode, error);
}

QString ConfigRepositoryAdapter::readClashDefaultMode(const QString& configPath) const {
  return ConfigManager::instance().readClashDefaultMode(configPath);
}

QJsonObject ConfigRepositoryAdapter::loadConfig(const QString& path) {
  return ConfigManager::instance().loadConfig(path);
}

bool ConfigRepositoryAdapter::saveConfig(const QString& path, const QJsonObject& config) {
  return ConfigManager::instance().saveConfig(path, config);
}

void ConfigRepositoryAdapter::applySettingsToConfig(QJsonObject& config) {
  ConfigManager::instance().applySettingsToConfig(config);
}

void ConfigRepositoryAdapter::applyPortSettings(QJsonObject& config) {
  ConfigManager::instance().applyPortSettings(config);
}

int ConfigRepositoryAdapter::mixedPort() const {
  return ConfigManager::instance().getMixedPort();
}
