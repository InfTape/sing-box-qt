#ifndef SHAREDRULESSTORE_H
#define SHAREDRULESSTORE_H

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

// 支持多规则集：文件结构 { "sets": [ { "name": "default", "rules": [ ... ] }, ... ] }
class SharedRulesStore
{
public:
    static QStringList listRuleSets();
    static QJsonArray loadRules(const QString &setName);
    static bool saveRules(const QString &setName, const QJsonArray &rules);

    static bool addRule(const QString &setName, const QJsonObject &rule);
    static bool replaceRule(const QString &setName, const QJsonObject &oldRule, const QJsonObject &newRule);
    static bool removeRule(const QString &setName, const QJsonObject &rule);
    static bool removeRuleFromAll(const QJsonObject &rule);

    static QString findSetOfRule(const QJsonObject &rule);

private:
    static QString storagePath();
    static QJsonObject loadDocument();
    static bool saveDocument(const QJsonObject &doc);
    static bool ruleEquals(const QJsonObject &a, const QJsonObject &b);
    static void normalizeRule(QJsonObject *rule);
};

#endif // SHAREDRULESSTORE_H
