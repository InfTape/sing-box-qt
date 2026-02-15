#include "services/rules/RuleConfigService.h"
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QtMath>
#include "app/interfaces/ConfigRepository.h"
#include "services/rules/SharedRulesStore.h"
#include "storage/DatabaseService.h"
#include "utils/rule/RuleUtils.h"

namespace {
QStringList splitBracketListValues(const QString& raw) {
  QStringList values;
  QString     token;
  bool        inQuote = false;
  QChar       quoteChar;

  auto pushToken = [&values, &token]() {
    const QString trimmed = token.trimmed();
    if (!trimmed.isEmpty()) {
      values << trimmed;
    }
    token.clear();
  };

  for (int i = 0; i < raw.size(); ++i) {
    const QChar ch = raw.at(i);
    if (inQuote) {
      if (ch == quoteChar) {
        inQuote = false;
      } else if (ch == '\\' && i + 1 < raw.size() &&
                 raw.at(i + 1) == quoteChar) {
        token += quoteChar;
        ++i;
      } else {
        token += ch;
      }
      continue;
    }
    if (ch == '"' || ch == '\'') {
      inQuote  = true;
      quoteChar = ch;
      continue;
    }
    if (ch == ',' || ch.isSpace()) {
      pushToken();
      continue;
    }
    token += ch;
  }

  pushToken();
  return values;
}

bool buildRouteRule(const RuleConfigService::RuleEditData& data,
                    QJsonObject*                           out,
                    QString*                               error) {
  if (!out) {
    return false;
  }
  const QString key = data.field.key.trimmed();
  if (key.isEmpty()) {
    if (error) {
      *error = QObject::tr("Match type cannot be empty.");
    }
    return false;
  }
  QStringList values = data.values;
  values.removeAll(QString());
  if (values.isEmpty()) {
    if (error) {
      *error = QObject::tr("Match value cannot be empty.");
    }
    return false;
  }
  QJsonObject rule;
  if (key == "ip_is_private") {
    if (values.size() != 1) {
      if (error) {
        *error =
            QObject::tr("ip_is_private allows only one value (true/false).");
      }
      return false;
    }
    const QString raw = values.first().toLower();
    if (raw != "true" && raw != "false") {
      if (error) {
        *error = QObject::tr("ip_is_private must be true or false.");
      }
      return false;
    }
    rule.insert(key, raw == "true");
  } else if (data.field.numeric) {
    QJsonArray arr;
    for (const auto& v : values) {
      bool      ok  = false;
      const int num = v.toInt(&ok);
      if (!ok) {
        if (error) {
          *error = QObject::tr("Port must be numeric: %1").arg(v);
        }
        return false;
      }
      arr.append(num);
    }
    if (arr.size() == 1) {
      rule.insert(key, arr.first());
    } else {
      rule.insert(key, arr);
    }
  } else {
    if (values.size() == 1) {
      rule.insert(key, values.first());
    } else {
      QJsonArray arr;
      for (const auto& v : values) {
        arr.append(v);
      }
      rule.insert(key, arr);
    }
  }
  rule["action"]   = "route";
  rule["outbound"] = data.outboundTag.trimmed();
  *out             = rule;
  return true;
}

int findInsertIndex(const QJsonArray& rules) {
  auto findIndex = [&rules](const QString& mode) -> int {
    for (int i = 0; i < rules.size(); ++i) {
      if (!rules[i].isObject()) {
        continue;
      }
      const QJsonObject obj = rules[i].toObject();
      if (obj.value("clash_mode").toString() == mode) {
        return i;
      }
    }
    return -1;
  };
  const int directIndex = findIndex("direct");
  const int globalIndex = findIndex("global");
  int       insertIndex = rules.size();
  if (directIndex >= 0 && globalIndex >= 0) {
    insertIndex = qMax(directIndex, globalIndex) + 1;
  } else if (directIndex >= 0) {
    insertIndex = directIndex + 1;
  } else if (globalIndex >= 0) {
    insertIndex = globalIndex + 1;
  } else if (!rules.isEmpty()) {
    insertIndex = 0;
  }
  return insertIndex;
}

QStringList comparableValuesFromJson(const QJsonValue& value) {
  QStringList out;
  if (value.isArray()) {
    const QJsonArray arr = value.toArray();
    out.reserve(arr.size());
    for (const auto& item : arr) {
      if (item.isDouble()) {
        out << QString::number(item.toInt());
      } else if (item.isBool()) {
        out << (item.toBool() ? QStringLiteral("true")
                              : QStringLiteral("false"));
      } else {
        out << item.toString().trimmed();
      }
    }
    return out;
  }
  if (value.isDouble()) {
    out << QString::number(value.toInt());
    return out;
  }
  if (value.isBool()) {
    out << (value.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
    return out;
  }
  const QString text = value.toString().trimmed();
  if (!text.isEmpty()) {
    out << text;
  }
  return out;
}

bool ruleObjectMatches(const QJsonObject& obj,
                       const RuleItem&    rule,
                       const QString&     key,
                       const QStringList& values) {
  if (obj.contains("action") && obj.value("action").toString() != "route") {
    return false;
  }
  if (!obj.contains(key)) {
    return false;
  }
  const QString objOutbound =
      RuleUtils::normalizeProxyValue(obj.value("outbound").toString());
  if (RuleUtils::normalizeProxyValue(rule.proxy) != objOutbound) {
    return false;
  }
  QStringList expected = values;
  for (QString& item : expected) {
    item = item.trimmed();
  }
  expected.removeAll(QString());
  expected.sort();
  QStringList actual = comparableValuesFromJson(obj.value(key));
  for (QString& item : actual) {
    item = item.trimmed();
  }
  actual.removeAll(QString());
  actual.sort();
  return !expected.isEmpty() && actual == expected;
}

QString normalizeLookupToken(QString value) {
  value = value.trimmed();
  if (value == "...") {
    return QString();
  }
  if (value.endsWith("...")) {
    value.chop(3);
    value = value.trimmed();
  }
  return value;
}

struct RuleLookupInput {
  QString     key;
  QStringList exactValuesSorted;
  QStringList truncatedTokens;
  QString     normalizedProxy;
  bool        hasTruncatedPayload = false;
};

bool buildRuleLookupInput(const RuleItem& rule, RuleLookupInput* out) {
  if (!out) {
    return false;
  }
  QString     key;
  QStringList values;
  if (!RuleConfigService::parseRulePayload(rule.payload, &key, &values, nullptr)) {
    return false;
  }
  out->key                 = key;
  out->normalizedProxy     = RuleUtils::normalizeProxyValue(rule.proxy);
  out->hasTruncatedPayload = rule.payload.contains("...");
  out->exactValuesSorted   = values;
  for (QString& value : out->exactValuesSorted) {
    value = value.trimmed();
  }
  out->exactValuesSorted.removeAll(QString());
  out->exactValuesSorted.sort();
  out->truncatedTokens.clear();
  for (const auto& value : values) {
    const QString token = normalizeLookupToken(value);
    if (!token.isEmpty()) {
      out->truncatedTokens << token;
    }
  }
  out->truncatedTokens.removeDuplicates();
  return !out->key.isEmpty() && !out->exactValuesSorted.isEmpty();
}

QStringList lookupComparableValues(const QJsonObject& obj, const QString& key) {
  if (!obj.contains(key)) {
    return {};
  }
  return comparableValuesFromJson(obj.value(key));
}

bool valuesMatchForLookup(const QStringList& actualValuesRaw,
                          const RuleLookupInput& lookup) {
  QStringList actualValues;
  actualValues.reserve(actualValuesRaw.size());
  for (const auto& value : actualValuesRaw) {
    const QString trimmed = value.trimmed();
    if (!trimmed.isEmpty()) {
      actualValues << trimmed;
    }
  }
  if (actualValues.isEmpty()) {
    return false;
  }
  QStringList sortedActual = actualValues;
  sortedActual.sort();
  if (sortedActual == lookup.exactValuesSorted) {
    return true;
  }
  if (!lookup.hasTruncatedPayload || lookup.truncatedTokens.isEmpty()) {
    return false;
  }
  for (const auto& token : lookup.truncatedTokens) {
    if (!actualValues.contains(token)) {
      return false;
    }
  }
  return true;
}

bool ruleObjectMatchesForLookup(const QJsonObject& obj,
                                const RuleLookupInput& lookup,
                                bool compareOutbound) {
  if (obj.contains("action") && obj.value("action").toString() != "route") {
    return false;
  }
  if (!obj.contains(lookup.key)) {
    return false;
  }
  if (compareOutbound) {
    const QString objOutbound =
        RuleUtils::normalizeProxyValue(obj.value("outbound").toString());
    if (lookup.normalizedProxy != objOutbound) {
      return false;
    }
  }
  return valuesMatchForLookup(lookupComparableValues(obj, lookup.key), lookup);
}

bool removeRuleFromArray(QJsonArray*     rules,
                         const RuleItem& rule,
                         QString*        error) {
  if (!rules) {
    return false;
  }
  QString     key;
  QStringList values;
  if (!RuleConfigService::parseRulePayload(
          rule.payload, &key, &values, error)) {
    return false;
  }
  bool removed = false;
  for (int i = 0; i < rules->size(); ++i) {
    if (!(*rules)[i].isObject()) {
      continue;
    }
    if (ruleObjectMatches((*rules)[i].toObject(), rule, key, values)) {
      rules->removeAt(i);
      removed = true;
      break;
    }
  }
  if (!removed && error) {
    *error = QObject::tr("Rule not found in config.");
  }
  return removed;
}

QString findRuleSetByPayload(const RuleItem& rule) {
  RuleLookupInput lookup;
  if (!buildRuleLookupInput(rule, &lookup)) {
    return QString();
  }
  const QStringList  setNames = SharedRulesStore::listRuleSets();
  QList<QJsonArray>  rulesBySet;
  rulesBySet.reserve(setNames.size());
  for (const auto& setName : setNames) {
    rulesBySet.append(SharedRulesStore::loadRules(setName));
  }
  for (int pass = 0; pass < 2; ++pass) {
    const bool compareOutbound = (pass == 0);
    for (int i = 0; i < setNames.size(); ++i) {
      const QJsonArray& rules = rulesBySet[i];
      for (const auto& value : rules) {
        if (!value.isObject()) {
          continue;
        }
        if (ruleObjectMatchesForLookup(
                value.toObject(), lookup, compareOutbound)) {
          return setNames[i].trimmed();
        }
      }
    }
  }
  return QString();
}

QJsonObject buildRouteRuleFromItem(const RuleItem& rule, QString* error) {
  QString     key;
  QStringList values;
  if (!RuleConfigService::parseRulePayload(
          rule.payload, &key, &values, error)) {
    return QJsonObject();
  }
  RuleConfigService::RuleEditData data;
  data.field.key     = key;
  data.field.numeric = (key.contains("port"));
  data.values        = values;
  data.outboundTag   = RuleUtils::normalizeProxyValue(rule.proxy);
  data.ruleSet       = "default";
  QJsonObject obj;
  if (!buildRouteRule(data, &obj, error)) {
    return QJsonObject();
  }
  return obj;
}
}  // namespace

bool RuleConfigService::buildRouteRulePublic(const RuleEditData& data,
                                             QJsonObject*        out,
                                             QString*            error) {
  return buildRouteRule(data, out, error);
}

QList<RuleConfigService::RuleFieldInfo> RuleConfigService::fieldInfos() {
  return {
      {QObject::tr("Domain"), "domain", QObject::tr("Example: example.com")},
      {QObject::tr("Domain Suffix"),
       "domain_suffix",
       QObject::tr("Example: example.com")},
      {QObject::tr("Domain Keyword"),
       "domain_keyword",
       QObject::tr("Example: google")},
      {QObject::tr("Domain Regex"),
       "domain_regex",
       QObject::tr("Example: ^.*\\\\.example\\\\.com$")},
      {QObject::tr("IP CIDR"),
       "ip_cidr",
       QObject::tr("Example: 192.168.0.0/16")},
      {QObject::tr("Private IP"),
       "ip_is_private",
       QObject::tr("Example: true / false")},
      {QObject::tr("Source IP CIDR"),
       "source_ip_cidr",
       QObject::tr("Example: 10.0.0.0/8")},
      {QObject::tr("Port"), "port", QObject::tr("Example: 80,443"), true},
      {QObject::tr("Source Port"),
       "source_port",
       QObject::tr("Example: 80,443"),
       true},
      {QObject::tr("Port Range"),
       "port_range",
       QObject::tr("Example: 10000:20000")},
      {QObject::tr("Source Port Range"),
       "source_port_range",
       QObject::tr("Example: 10000:20000")},
      {QObject::tr("Process Name"),
       "process_name",
       QObject::tr("Example: chrome.exe")},
      {QObject::tr("Process Path"),
       "process_path",
       QObject::tr("Example: C:\\\\Program Files\\\\App\\\\app.exe")},
      {QObject::tr("Process Path Regex"),
       "process_path_regex",
       QObject::tr("Example: ^C:\\\\\\\\Program Files\\\\\\\\.+")},
      {QObject::tr("Package Name"),
       "package_name",
       QObject::tr("Example: com.example.app")},
  };
}

QString RuleConfigService::activeConfigPath(ConfigRepository* cfgRepo) {
  const QString subPath = DatabaseService::instance().getActiveConfigPath();
  if (!subPath.isEmpty()) {
    return subPath;
  }
  return cfgRepo ? cfgRepo->getActiveConfigPath() : QString();
}

QString RuleConfigService::findRuleSet(ConfigRepository* /*cfgRepo*/,
                                       const RuleItem& rule) {
  const QJsonObject obj = buildRouteRuleFromItem(rule, nullptr);
  const QString     setName = SharedRulesStore::findSetOfRule(obj);
  if (!setName.isEmpty()) {
    return setName;
  }
  return findRuleSetByPayload(rule);
}

QStringList RuleConfigService::loadOutboundTags(ConfigRepository* cfgRepo,
                                                const QString&    extraTag,
                                                QString*          error) {
  QSet<QString> tags;
  const QString path = activeConfigPath(cfgRepo);
  if (!path.isEmpty()) {
    const QJsonObject configOut =
        cfgRepo ? cfgRepo->loadConfig(path) : QJsonObject();
    if (configOut.isEmpty()) {
      if (error) {
        *error = QObject::tr("Failed to read config file: %1").arg(path);
      }
    } else if (configOut.value("outbounds").isArray()) {
      const QJsonArray outbounds = configOut.value("outbounds").toArray();
      for (const auto& item : outbounds) {
        if (!item.isObject()) {
          continue;
        }
        const QString tag = item.toObject().value("tag").toString().trimmed();
        if (!tag.isEmpty()) {
          tags.insert(tag);
        }
      }
    }
  } else if (error) {
    *error = QObject::tr("Active config not found.");
  }
  if (!extraTag.isEmpty()) {
    tags.insert(extraTag);
  }
  if (tags.isEmpty()) {
    tags.insert("direct");
  }
  QStringList list = tags.values();
  list.sort();
  return list;
}

bool RuleConfigService::addRule(ConfigRepository*   cfgRepo,
                                const RuleEditData& data,
                                RuleItem*           added,
                                QString*            error) {
  const QString path = activeConfigPath(cfgRepo);
  if (path.isEmpty()) {
    if (error) {
      *error = QObject::tr("Active config not found.");
    }
    return false;
  }
  if (!cfgRepo) {
    if (error) {
      *error = QObject::tr("Config service not available.");
    }
    return false;
  }
  QJsonObject config = cfgRepo->loadConfig(path);
  if (config.isEmpty()) {
    if (error) {
      *error = QObject::tr("Failed to read config file: %1").arg(path);
    }
    return false;
  }
  QJsonObject routeRule;
  if (!buildRouteRule(data, &routeRule, error)) {
    return false;
  }
  QJsonObject route             = config.value("route").toObject();
  QJsonArray  rules             = route.value("rules").toArray();
  bool        hasRouteRule      = false;
  int         existingRuleIndex = -1;
  for (int i = 0; i < rules.size(); ++i) {
    if (!rules[i].isObject()) {
      continue;
    }
    const QJsonObject ruleObj = rules[i].toObject();
    if (ruleObj == routeRule) {
      hasRouteRule      = true;
      existingRuleIndex = i;
      break;
    }
  }
  int insertIndex = findInsertIndex(rules);
  if (!hasRouteRule) {
    rules.insert(insertIndex, routeRule);
  } else if (existingRuleIndex >= 0 && existingRuleIndex > insertIndex) {
    QJsonObject existingRule = rules[existingRuleIndex].toObject();
    rules.removeAt(existingRuleIndex);
    rules.insert(insertIndex, existingRule);
  }
  route["rules"]  = rules;
  config["route"] = route;
  if (!cfgRepo->saveConfig(path, config)) {
    if (error) {
      *error = QObject::tr("Failed to save config: %1").arg(path);
    }
    return false;
  }
  const QString setName = data.ruleSet.isEmpty() ? "default" : data.ruleSet;
  if (added) {
    RuleItem item;
    item.type     = data.field.key;
    item.payload  = QString("%1=%2").arg(data.field.key, data.values.join(","));
    item.proxy    = data.outboundTag.trimmed();
    item.ruleSet  = setName;
    item.isCustom = true;
    *added        = item;
  }
  SharedRulesStore::addRule(setName, routeRule);
  return true;
}

bool RuleConfigService::updateRule(ConfigRepository*   cfgRepo,
                                   const RuleItem&     existing,
                                   const RuleEditData& data,
                                   RuleItem*           updated,
                                   QString*            error) {
  const QString path = activeConfigPath(cfgRepo);
  if (path.isEmpty()) {
    if (error) {
      *error = QObject::tr("Active config not found.");
    }
    return false;
  }
  if (!cfgRepo) {
    if (error) {
      *error = QObject::tr("Config service not available.");
    }
    return false;
  }
  QJsonObject config = cfgRepo->loadConfig(path);
  if (config.isEmpty()) {
    if (error) {
      *error = QObject::tr("Failed to read config file: %1").arg(path);
    }
    return false;
  }
  QJsonObject route = config.value("route").toObject();
  QJsonArray  rules = route.value("rules").toArray();
  if (!removeRuleFromArray(&rules, existing, error)) {
    return false;
  }
  QJsonObject routeRule;
  if (!buildRouteRule(data, &routeRule, error)) {
    return false;
  }
  const int insertIndex = findInsertIndex(rules);
  rules.insert(insertIndex, routeRule);
  route["rules"]  = rules;
  config["route"] = route;
  if (!cfgRepo->saveConfig(path, config)) {
    if (error) {
      *error = QObject::tr("Failed to save config");
    }
    return false;
  }
  const QString targetSet  = data.ruleSet.isEmpty() ? "default" : data.ruleSet;
  if (updated) {
    RuleItem item;
    item.type     = data.field.key;
    item.payload  = QString("%1=%2").arg(data.field.key, data.values.join(","));
    item.proxy    = RuleUtils::normalizeProxyValue(data.outboundTag);
    item.ruleSet  = targetSet;
    item.isCustom = true;
    *updated      = item;
  }
  const QJsonObject oldRouteRule = buildRouteRuleFromItem(existing, nullptr);
  const QString     oldSet = SharedRulesStore::findSetOfRule(oldRouteRule);
  if (!oldSet.isEmpty() && oldSet != targetSet) {
    SharedRulesStore::removeRule(oldSet, oldRouteRule);
  }
  SharedRulesStore::replaceRule(targetSet, oldRouteRule, routeRule);
  return true;
}

bool RuleConfigService::removeRule(ConfigRepository* cfgRepo,
                                   const RuleItem&   rule,
                                   QString*          error) {
  const QString path = activeConfigPath(cfgRepo);
  if (path.isEmpty()) {
    if (error) {
      *error = QObject::tr("Active config not found.");
    }
    return false;
  }
  if (!cfgRepo) {
    if (error) {
      *error = QObject::tr("Config service not available.");
    }
    return false;
  }
  QJsonObject config = cfgRepo->loadConfig(path);
  if (config.isEmpty()) {
    if (error) {
      *error = QObject::tr("Failed to read config file: %1").arg(path);
    }
    return false;
  }
  QJsonObject route = config.value("route").toObject();
  QJsonArray  rules = route.value("rules").toArray();
  if (!removeRuleFromArray(&rules, rule, error)) {
    return false;
  }
  route["rules"]  = rules;
  config["route"] = route;
  if (!cfgRepo->saveConfig(path, config)) {
    if (error) {
      *error = QObject::tr("Failed to save config: %1").arg(path);
    }
    return false;
  }
  const QJsonObject oldRouteRule = buildRouteRuleFromItem(rule, nullptr);
  const QString     set = SharedRulesStore::findSetOfRule(oldRouteRule);
  if (!set.isEmpty()) {
    SharedRulesStore::removeRule(set, oldRouteRule);
  } else {
    SharedRulesStore::removeRuleFromAll(oldRouteRule);
  }
  return true;
}

bool RuleConfigService::parseRulePayload(const QString& payload,
                                         QString*       key,
                                         QStringList*   values,
                                         QString*       error) {
  if (key) {
    key->clear();
  }
  if (values) {
    values->clear();
  }
  const QString trimmed = payload.trimmed();
  const int     eq      = trimmed.indexOf('=');
  if (eq <= 0) {
    if (error) {
      *error = QObject::tr("Failed to parse current rule content.");
    }
    return false;
  }
  const QString foundKey     = trimmed.left(eq).trimmed();
  const QString valueStr     = trimmed.mid(eq + 1).trimmed();
  QStringList   parsedValues;
  if (valueStr.startsWith('[') && valueStr.endsWith(']')) {
    const QString bracketInner =
        valueStr.mid(1, valueStr.size() - 2).trimmed();
    parsedValues = splitBracketListValues(bracketInner);
  } else {
    parsedValues = valueStr.split(',', Qt::SkipEmptyParts);
  }
  for (QString& v : parsedValues) {
    v = v.trimmed();
  }
  parsedValues.removeAll(QString());
  if (parsedValues.isEmpty()) {
    if (error) {
      *error = QObject::tr("Match value cannot be empty.");
    }
    return false;
  }
  if (key) {
    *key = foundKey;
  }
  if (values) {
    *values = parsedValues;
  }
  return true;
}
