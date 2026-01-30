#include "services/SharedRulesStore.h"
#include "storage/ConfigIO.h"
#include "utils/Logger.h"
#include <QFile>
#include <QJsonDocument>

namespace {
QString ruleToString(const QJsonObject &obj)
{
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}
} // namespace

QString SharedRulesStore::storagePath()
{
    return ConfigIO::getConfigDir() + "/shared-rules.json";
}

QJsonObject SharedRulesStore::loadDocument()
{
    QFile file(storagePath());
    if (!file.exists()) {
        return QJsonObject{{"sets", QJsonArray()}};
    }
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::warn(QString("Failed to open shared rules file: %1").arg(storagePath()));
        return QJsonObject{{"sets", QJsonArray()}};
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    // 兼容旧版：若根是数组则视为默认规则集
    if (doc.isArray()) {
        QJsonObject obj;
        QJsonObject set;
        set["name"] = "default";
        set["rules"] = doc.array();
        obj["sets"] = QJsonArray{set};
        return obj;
    }

    if (!doc.isObject()) {
        Logger::warn("Shared rules file invalid, reset to empty.");
        return QJsonObject{{"sets", QJsonArray()}};
    }

    QJsonObject root = doc.object();
    if (!root.contains("sets") || !root.value("sets").isArray()) {
        root["sets"] = QJsonArray();
    }
    return root;
}

bool SharedRulesStore::saveDocument(const QJsonObject &doc)
{
    QFile file(storagePath());
    if (!file.open(QIODevice::WriteOnly)) {
        Logger::error(QString("Failed to write shared rules file: %1").arg(storagePath()));
        return false;
    }
    file.write(QJsonDocument(doc).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool SharedRulesStore::ruleEquals(const QJsonObject &a, const QJsonObject &b)
{
    return ruleToString(a) == ruleToString(b);
}

void SharedRulesStore::normalizeRule(QJsonObject *rule)
{
    if (!rule) return;
    rule->insert("shared", true); // internal marker, will be stripped before writing config
    if (!rule->contains("source")) {
        rule->insert("source", "custom");
    }
}

QStringList SharedRulesStore::listRuleSets()
{
    QJsonObject doc = loadDocument();
    QStringList names;
    for (const auto &v : doc.value("sets").toArray()) {
        if (!v.isObject()) continue;
        const QString name = v.toObject().value("name").toString();
        if (!name.isEmpty()) names << name;
    }
    names.removeDuplicates();
    names.sort();
    if (names.isEmpty()) names << "default";
    return names;
}

bool SharedRulesStore::ensureRuleSet(const QString &name)
{
    const QString target = name.trimmed().isEmpty() ? "default" : name.trimmed();
    QJsonObject doc = loadDocument();
    QJsonArray sets = doc.value("sets").toArray();
    for (const auto &v : sets) {
        if (v.isObject() && v.toObject().value("name").toString() == target) {
            return true;
        }
    }
    QJsonObject set;
    set["name"] = target;
    set["rules"] = QJsonArray();
    sets.append(set);
    doc["sets"] = sets;
    return saveDocument(doc);
}

bool SharedRulesStore::removeRuleSet(const QString &name)
{
    const QString target = name.trimmed();
    if (target.isEmpty() || target == "default") return false;

    QJsonObject doc = loadDocument();
    QJsonArray sets = doc.value("sets").toArray();
    QJsonArray kept;
    bool removed = false;
    for (const auto &v : sets) {
        if (!v.isObject()) {
            kept.append(v);
            continue;
        }
        const QJsonObject set = v.toObject();
        if (set.value("name").toString() == target) {
            removed = true;
            continue;
        }
        kept.append(v);
    }
    if (!removed) return false;
    doc["sets"] = kept;
    return saveDocument(doc);
}

bool SharedRulesStore::renameRuleSet(const QString &from, const QString &to)
{
    const QString src = from.trimmed();
    const QString dst = to.trimmed();
    if (src.isEmpty() || dst.isEmpty() || src == "default") return false;

    QJsonObject doc = loadDocument();
    QJsonArray sets = doc.value("sets").toArray();
    bool renamed = false;
    for (int i = 0; i < sets.size(); ++i) {
        if (!sets[i].isObject()) continue;
        QJsonObject set = sets[i].toObject();
        const QString name = set.value("name").toString();
        if (name == dst) return false; // duplicate
        if (name == src) {
            set["name"] = dst;
            sets[i] = set;
            renamed = true;
        }
    }
    if (!renamed) return false;
    doc["sets"] = sets;
    return saveDocument(doc);
}

QJsonArray SharedRulesStore::loadRules(const QString &setName)
{
    const QString target = setName.isEmpty() ? "default" : setName;
    QJsonObject doc = loadDocument();
    for (const auto &v : doc.value("sets").toArray()) {
        if (!v.isObject()) continue;
        const QJsonObject set = v.toObject();
        if (set.value("name").toString() == target) {
            return set.value("rules").toArray();
        }
    }
    return QJsonArray();
}

bool SharedRulesStore::saveRules(const QString &setName, const QJsonArray &rules)
{
    const QString target = setName.isEmpty() ? "default" : setName;
    QJsonObject doc = loadDocument();
    QJsonArray sets = doc.value("sets").toArray();
    bool found = false;
    for (int i = 0; i < sets.size(); ++i) {
        if (!sets[i].isObject()) continue;
        QJsonObject set = sets[i].toObject();
        if (set.value("name").toString() == target) {
            set["name"] = target;
            set["rules"] = rules;
            sets[i] = set;
            found = true;
            break;
        }
    }
    if (!found) {
        QJsonObject set;
        set["name"] = target;
        set["rules"] = rules;
        sets.append(set);
    }
    doc["sets"] = sets;
    return saveDocument(doc);
}

bool SharedRulesStore::addRule(const QString &setName, const QJsonObject &rule)
{
    QJsonObject normalized = rule;
    normalizeRule(&normalized);
    const QString target = setName.trimmed().isEmpty() ? "default" : setName.trimmed();
    QJsonArray rules = loadRules(target);
    for (const auto &val : rules) {
        if (!val.isObject()) continue;
        if (ruleEquals(val.toObject(), normalized)) {
            return true;
        }
    }
    rules.append(normalized);
    return saveRules(target, rules);
}

bool SharedRulesStore::replaceRule(const QString &setName, const QJsonObject &oldRule, const QJsonObject &newRule)
{
    QJsonObject oldNormalized = oldRule;
    QJsonObject newNormalized = newRule;
    normalizeRule(&oldNormalized);
    normalizeRule(&newNormalized);

    const QString target = setName.trimmed().isEmpty() ? "default" : setName.trimmed();
    QJsonArray rules = loadRules(target);
    bool replaced = false;
    for (int i = 0; i < rules.size(); ++i) {
        if (!rules[i].isObject()) continue;
        if (ruleEquals(rules[i].toObject(), oldNormalized)) {
            rules[i] = newNormalized;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        rules.append(newNormalized);
    }
    return saveRules(target, rules);
}

bool SharedRulesStore::removeRule(const QString &setName, const QJsonObject &rule)
{
    QJsonObject normalized = rule;
    normalizeRule(&normalized);

    const QString target = setName.trimmed().isEmpty() ? "default" : setName.trimmed();
    QJsonArray rules = loadRules(target);
    for (int i = 0; i < rules.size(); ++i) {
        if (!rules[i].isObject()) continue;
        if (ruleEquals(rules[i].toObject(), normalized)) {
            rules.removeAt(i);
            return saveRules(target, rules);
        }
    }
    return true;
}

bool SharedRulesStore::removeRuleFromAll(const QJsonObject &rule)
{
    QJsonObject doc = loadDocument();
    QJsonArray sets = doc.value("sets").toArray();
    QJsonObject normalized = rule;
    normalizeRule(&normalized);

    bool changed = false;
    for (int i = 0; i < sets.size(); ++i) {
        if (!sets[i].isObject()) continue;
        QJsonObject set = sets[i].toObject();
        QJsonArray rules = set.value("rules").toArray();
        for (int j = 0; j < rules.size(); ++j) {
            if (!rules[j].isObject()) continue;
            if (ruleEquals(rules[j].toObject(), normalized)) {
                rules.removeAt(j);
                changed = true;
                break;
            }
        }
        set["rules"] = rules;
        sets[i] = set;
    }

    if (changed) {
        doc["sets"] = sets;
        return saveDocument(doc);
    }
    return true;
}

QString SharedRulesStore::findSetOfRule(const QJsonObject &rule)
{
    QJsonObject normalized = rule;
    normalizeRule(&normalized);
    QJsonObject doc = loadDocument();
    for (const auto &v : doc.value("sets").toArray()) {
        if (!v.isObject()) continue;
        const QJsonObject set = v.toObject();
        const QJsonArray rules = set.value("rules").toArray();
        for (const auto &r : rules) {
            if (!r.isObject()) continue;
            if (ruleEquals(r.toObject(), normalized)) {
                return set.value("name").toString();
            }
        }
    }
    return QString();
}
