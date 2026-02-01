#ifndef RULECONFIGSERVICE_H
#define RULECONFIGSERVICE_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include "models/RuleItem.h"

class ConfigRepository;

class RuleConfigService
{
public:
    struct RuleFieldInfo {
        QString label;
        QString key;
        QString placeholder;
        bool numeric = false;
    };

    struct RuleEditData {
        RuleFieldInfo field;
        QStringList values;
        QString outboundTag;
        QString ruleSet; // 多规则集名称
    };

    static QList<RuleFieldInfo> fieldInfos();
    static QString activeConfigPath(ConfigRepository *cfgRepo);
    static QStringList loadOutboundTags(ConfigRepository *cfgRepo, const QString &extraTag = QString(), QString *error = nullptr);

    static bool addRule(ConfigRepository *cfgRepo, const RuleEditData &data, RuleItem *added, QString *error);
    static bool updateRule(ConfigRepository *cfgRepo, const RuleItem &existing, const RuleEditData &data, RuleItem *updated, QString *error);
    static bool removeRule(ConfigRepository *cfgRepo, const RuleItem &rule, QString *error);
    static QString findRuleSet(ConfigRepository *cfgRepo, const RuleItem &rule);

    static bool parseRulePayload(const QString &payload, QString *key, QStringList *values, QString *error = nullptr);
    // 公共构建接口（供规则集管理对话框使用）
    static bool buildRouteRulePublic(const RuleEditData &data, QJsonObject *out, QString *error = nullptr);
};

#endif // RULECONFIGSERVICE_H
