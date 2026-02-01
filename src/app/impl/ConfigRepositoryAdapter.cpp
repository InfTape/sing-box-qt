#include "ConfigRepositoryAdapter.h"

#include "services/config/ConfigManager.h"

QString ConfigRepositoryAdapter::getActiveConfigPath() const
{
    return ConfigManager::instance().getActiveConfigPath();
}

bool ConfigRepositoryAdapter::generateConfigWithNodes(const QJsonArray &nodes, const QString &targetPath)
{
    return ConfigManager::instance().generateConfigWithNodes(nodes, targetPath);
}

bool ConfigRepositoryAdapter::updateClashDefaultMode(const QString &configPath, const QString &mode, QString *error)
{
    return ConfigManager::instance().updateClashDefaultMode(configPath, mode, error);
}

QString ConfigRepositoryAdapter::readClashDefaultMode(const QString &configPath) const
{
    return ConfigManager::instance().readClashDefaultMode(configPath);
}
