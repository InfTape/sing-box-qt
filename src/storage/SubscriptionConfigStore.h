#ifndef SUBSCRIPTIONCONFIGSTORE_H
#define SUBSCRIPTIONCONFIGSTORE_H

#include <QJsonArray>
#include <QString>

namespace SubscriptionConfigStore {
QString generateConfigFileName(const QString &name);
bool saveConfigWithNodes(const QJsonArray &nodes, const QString &targetPath);
bool saveOriginalConfig(const QString &content, const QString &targetPath);
bool rollbackSubscriptionConfig(const QString &configPath);
bool deleteSubscriptionConfig(const QString &configPath);
}

#endif // SUBSCRIPTIONCONFIGSTORE_H
