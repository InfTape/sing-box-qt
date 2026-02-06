#ifndef SUBSCRIPTIONCONFIGSTORE_H
#define SUBSCRIPTIONCONFIGSTORE_H
#include <QJsonArray>
#include <QString>
class ConfigRepository;

namespace SubscriptionConfigStore {
QString generateConfigFileName(const QString& name);
bool    saveConfigWithNodes(ConfigRepository* cfgRepo, const QJsonArray& nodes, const QString& targetPath);
bool    saveOriginalConfig(ConfigRepository* cfgRepo, const QString& content, const QString& targetPath);
bool    rollbackSubscriptionConfig(const QString& configPath);
bool    deleteSubscriptionConfig(const QString& configPath);
}  // namespace SubscriptionConfigStore
#endif  // SUBSCRIPTIONCONFIGSTORE_H
