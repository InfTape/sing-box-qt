#include "services/config/ConfigMutator.h"
#include <QDir>
#include <QHostAddress>
#include <QSet>
#include <QStringList>
#include <limits>
#include "services/config/ConfigBuilder.h"
#include "storage/AppSettings.h"
#include "storage/ConfigConstants.h"
#include "utils/AppPaths.h"
#include "utils/Logger.h"

namespace {
bool isIpAddress(const QString& value) {
  QHostAddress addr;
  return addr.setAddress(value);
}

bool shouldIncludeNodeInGroups(const QJsonObject& node) {
  const QString server = node.value("server").toString().trimmed();
  if (server.isEmpty()) {
    return false;
  }
  if (server == "0.0.0.0") {
    return false;
  }
  return true;
}

int ensureOutboundIndex(QJsonArray& outbounds, const QString& tag) {
  for (int i = 0; i < outbounds.size(); ++i) {
    if (!outbounds[i].isObject()) continue;
    if (outbounds[i].toObject().value("tag").toString() == tag) {
      return i;
    }
  }
  QJsonObject created;
  created["tag"] = tag;
  outbounds.append(created);
  return outbounds.size() - 1;
}

void updateUrltestAndSelector(QJsonArray& outbounds, const QStringList& nodeTags) {
  const AppSettings& settings     = AppSettings::instance();
  const int          autoIdx      = ensureOutboundIndex(outbounds, ConfigConstants::TAG_AUTO);
  const int          manualIdx    = ensureOutboundIndex(outbounds, ConfigConstants::TAG_MANUAL);
  QJsonObject        autoOutbound = outbounds[autoIdx].toObject();
  autoOutbound["type"]            = "urltest";
  autoOutbound["tag"]             = ConfigConstants::TAG_AUTO;
  if (nodeTags.isEmpty()) {
    autoOutbound["outbounds"] = QJsonArray::fromStringList(QStringList() << ConfigConstants::TAG_DIRECT);
  } else {
    autoOutbound["outbounds"] = QJsonArray::fromStringList(nodeTags);
  }
  autoOutbound["interrupt_exist_connections"] = true;
  autoOutbound["idle_timeout"]                = "10m";
  autoOutbound["url"]                         = settings.urltestUrl();
  autoOutbound["interval"]                    = "10m";
  autoOutbound["tolerance"]                   = 50;
  outbounds[autoIdx]                          = autoOutbound;
  QJsonObject manualOutbound                  = outbounds[manualIdx].toObject();
  manualOutbound["type"]                      = "selector";
  manualOutbound["tag"]                       = ConfigConstants::TAG_MANUAL;
  QStringList manualList;
  manualList << ConfigConstants::TAG_AUTO;
  for (const auto& tag : nodeTags) {
    manualList << tag;
  }
  manualOutbound["outbounds"] = QJsonArray::fromStringList(manualList);
  outbounds[manualIdx]        = manualOutbound;
}

void updateAppGroupSelectors(QJsonArray& outbounds, const QStringList& nodeTags) {
  const QStringList groups = {ConfigConstants::TAG_TELEGRAM, ConfigConstants::TAG_YOUTUBE, ConfigConstants::TAG_NETFLIX,
                              ConfigConstants::TAG_OPENAI};
  for (const auto& groupTag : groups) {
    int idx = -1;
    for (int i = 0; i < outbounds.size(); ++i) {
      if (!outbounds[i].isObject()) continue;
      if (outbounds[i].toObject().value("tag").toString() == groupTag) {
        idx = i;
        break;
      }
    }
    if (idx < 0) {
      continue;
    }
    QStringList groupList;
    groupList << ConfigConstants::TAG_MANUAL << ConfigConstants::TAG_AUTO;
    for (const auto& tag : nodeTags) {
      groupList << tag;
    }
    QJsonObject group  = outbounds[idx].toObject();
    group["outbounds"] = QJsonArray::fromStringList(groupList);
    outbounds[idx]     = group;
  }
}

int findInsertIndex(const QJsonArray& rules) {
  auto findIndex = [&rules](const QString& mode) -> int {
    for (int i = 0; i < rules.size(); ++i) {
      if (!rules[i].isObject()) continue;
      const QJsonObject obj = rules[i].toObject();
      if (obj.value("clash_mode").toString() == mode) return i;
    }
    return -1;
  };
  const int directIndex = findIndex("direct");
  const int globalIndex = findIndex("global");
  int       insertIndex = rules.size();
  if (directIndex >= 0 && globalIndex >= 0)
    insertIndex = qMax(directIndex, globalIndex) + 1;
  else if (directIndex >= 0)
    insertIndex = directIndex + 1;
  else if (globalIndex >= 0)
    insertIndex = globalIndex + 1;
  else if (!rules.isEmpty())
    insertIndex = 0;
  return insertIndex;
}

void normalizeCacheFileConfig(QJsonObject& experimental) {
  QJsonObject cacheFile = experimental.value("cache_file").toObject();
  cacheFile["enabled"]  = cacheFile.value("enabled").toBool(true);
  if (cacheFile.value("path").toString().trimmed().isEmpty()) {
    cacheFile["path"] = QDir(appDataDir()).filePath("cache.db");
  }
  experimental["cache_file"] = cacheFile;
}
}  // namespace

bool ConfigMutator::injectNodes(QJsonObject& config, const QJsonArray& nodes) {
  QJsonArray    outbounds = config.value("outbounds").toArray();
  QSet<QString> existingTags;
  for (const auto& obVal : outbounds) {
    if (!obVal.isObject()) continue;
    const QJsonObject ob  = obVal.toObject();
    const QString     tag = ob.value("tag").toString().trimmed();
    if (!tag.isEmpty()) {
      existingTags.insert(tag);
    }
  }
  QStringList   groupNodeTags;
  QJsonArray    normalizedNodes;
  const QString resolverStrategy = AppSettings::instance().dnsStrategy();
  for (int i = 0; i < nodes.size(); ++i) {
    if (!nodes[i].isObject()) {
      Logger::warn(QString("Skip node: not an object, index=%1").arg(i));
      continue;
    }
    QJsonObject   node   = nodes[i].toObject();
    const QString rawTag = node.value("tag").toString().trimmed();
    if (rawTag.isEmpty()) {
      Logger::warn(QString("Skip node: missing tag, index=%1").arg(i));
      continue;
    }
    if (node.value("type").toString().trimmed().isEmpty()) {
      Logger::warn(QString("Skip node: missing type, tag=%1, index=%2").arg(rawTag).arg(i));
      continue;
    }
    QString tag = rawTag;
    if (existingTags.contains(tag)) {
      QString candidate = QString("node-%1-%2").arg(rawTag).arg(i);
      if (!existingTags.contains(candidate)) {
        tag = candidate;
      } else {
        int counter = 1;
        while (true) {
          candidate = QString("node-%1-%2").arg(rawTag).arg(counter);
          if (!existingTags.contains(candidate)) {
            tag = candidate;
            break;
          }
          if (counter < std::numeric_limits<int>::max()) {
            counter += 1;
          }
        }
      }
    }
    existingTags.insert(tag);
    node["tag"]          = tag;
    const QString server = node.value("server").toString().trimmed();
    if (!server.isEmpty() && server != "0.0.0.0" && !isIpAddress(server) && !node.contains("domain_resolver")) {
      QJsonObject resolver;
      resolver["server"]      = ConfigConstants::DNS_RESOLVER;
      resolver["strategy"]    = resolverStrategy;
      node["domain_resolver"] = resolver;
    }
    if (shouldIncludeNodeInGroups(node)) {
      groupNodeTags.append(tag);
    }
    normalizedNodes.append(node);
  }
  updateUrltestAndSelector(outbounds, groupNodeTags);
  updateAppGroupSelectors(outbounds, groupNodeTags);
  for (const auto& nodeVal : normalizedNodes) {
    outbounds.append(nodeVal);
  }
  config["outbounds"] = outbounds;
  return true;
}

void ConfigMutator::applySettings(QJsonObject& config) {
  const AppSettings& settings             = AppSettings::instance();
  config["inbounds"]                      = ConfigBuilder::buildInbounds();
  QJsonObject experimental                = config.value("experimental").toObject();
  QJsonObject clashApi                    = experimental.value("clash_api").toObject();
  clashApi["external_controller"]         = QString("127.0.0.1:%1").arg(settings.apiPort());
  clashApi["external_ui_download_detour"] = settings.normalizedDownloadDetour();
  experimental["clash_api"]               = clashApi;
  normalizeCacheFileConfig(experimental);
  config["experimental"] = experimental;
  QJsonObject dns        = config.value("dns").toObject();
  dns["strategy"]        = settings.dnsStrategy();
  if (dns.contains("servers") && dns["servers"].isArray()) {
    QJsonArray servers = dns.value("servers").toArray();
    for (int i = 0; i < servers.size(); ++i) {
      if (!servers[i].isObject()) continue;
      QJsonObject   server = servers[i].toObject();
      const QString tag    = server.value("tag").toString();
      if (tag == ConfigConstants::DNS_PROXY) {
        server["address"] = settings.dnsProxy();
        server["detour"]  = settings.normalizedDefaultOutbound();
      } else if (tag == ConfigConstants::DNS_CN) {
        server["address"] = settings.dnsCn();
      } else if (tag == ConfigConstants::DNS_RESOLVER) {
        server["address"] = settings.dnsResolver();
      }
      servers[i] = server;
    }
    dns["servers"] = servers;
  }
  if (dns.contains("rules") && dns["rules"].isArray()) {
    QJsonArray rules    = dns.value("rules").toArray();
    int        adsIndex = -1;
    for (int i = 0; i < rules.size(); ++i) {
      if (!rules[i].isObject()) continue;
      const QJsonObject rule = rules[i].toObject();
      if (rule.value("rule_set").toString() == ConfigConstants::RS_GEOSITE_ADS) {
        adsIndex = i;
        break;
      }
    }
    if (settings.blockAds()) {
      if (adsIndex < 0) {
        QJsonObject adsRule;
        adsRule["rule_set"] = ConfigConstants::RS_GEOSITE_ADS;
        adsRule["server"]   = ConfigConstants::DNS_BLOCK;
        rules.insert(0, adsRule);
      } else {
        QJsonObject rule = rules[adsIndex].toObject();
        rule["server"]   = ConfigConstants::DNS_BLOCK;
        rules[adsIndex]  = rule;
      }
    } else if (adsIndex >= 0) {
      rules.removeAt(adsIndex);
    }
    dns["rules"] = rules;
  }
  config["dns"] = dns;
  if (config.contains("outbounds") && config["outbounds"].isArray()) {
    QJsonArray outbounds = config.value("outbounds").toArray();
    for (int i = 0; i < outbounds.size(); ++i) {
      if (!outbounds[i].isObject()) continue;
      QJsonObject ob = outbounds[i].toObject();
      if (ob.value("tag").toString() == ConfigConstants::TAG_AUTO) {
        ob["interrupt_exist_connections"] = true;
        ob["idle_timeout"]                = "10m";
        ob["url"]                         = settings.urltestUrl();
        outbounds[i]                      = ob;
      }
    }
    if (!settings.enableAppGroups()) {
      QJsonArray filtered;
      for (const auto& obVal : outbounds) {
        if (!obVal.isObject()) {
          filtered.append(obVal);
          continue;
        }
        const QString tag = obVal.toObject().value("tag").toString();
        if (tag != ConfigConstants::TAG_TELEGRAM && tag != ConfigConstants::TAG_YOUTUBE &&
            tag != ConfigConstants::TAG_NETFLIX && tag != ConfigConstants::TAG_OPENAI) {
          filtered.append(obVal);
        }
      }
      outbounds = filtered;
    }
    config["outbounds"] = outbounds;
  }
  if (config.contains("route") && config["route"].isObject()) {
    QJsonObject route                = config.value("route").toObject();
    route["final"]                   = settings.normalizedDefaultOutbound();
    route["default_domain_resolver"] = ConfigConstants::DNS_RESOLVER;
    if (route.contains("rule_set") && route["rule_set"].isArray()) {
      QJsonArray ruleSets = route.value("rule_set").toArray();
      for (int i = 0; i < ruleSets.size(); ++i) {
        if (!ruleSets[i].isObject()) continue;
        QJsonObject rs = ruleSets[i].toObject();
        if (rs.value("type").toString() == "remote") {
          rs["download_detour"] = settings.normalizedDownloadDetour();
        }
        ruleSets[i] = rs;
      }
      if (!settings.blockAds()) {
        QJsonArray filtered;
        for (const auto& rsVal : ruleSets) {
          if (!rsVal.isObject()) {
            filtered.append(rsVal);
            continue;
          }
          if (rsVal.toObject().value("tag").toString() != ConfigConstants::RS_GEOSITE_ADS) {
            filtered.append(rsVal);
          }
        }
        ruleSets = filtered;
      }
      if (!settings.enableAppGroups()) {
        QJsonArray filtered;
        for (const auto& rsVal : ruleSets) {
          if (!rsVal.isObject()) {
            filtered.append(rsVal);
            continue;
          }
          const QString tag = rsVal.toObject().value("tag").toString();
          if (tag != ConfigConstants::RS_GEOSITE_TELEGRAM && tag != ConfigConstants::RS_GEOSITE_YOUTUBE &&
              tag != ConfigConstants::RS_GEOSITE_NETFLIX && tag != ConfigConstants::RS_GEOSITE_OPENAI) {
            filtered.append(rsVal);
          }
        }
        ruleSets = filtered;
      }
      route["rule_set"] = ruleSets;
    }
    if (route.contains("rules") && route["rules"].isArray()) {
      QJsonArray rules = route.value("rules").toArray();
      for (int i = 0; i < rules.size(); ++i) {
        if (!rules[i].isObject()) continue;
        QJsonObject rule = rules[i].toObject();
        if (rule.value("clash_mode").toString() == "global") {
          rule["outbound"] = settings.normalizedDefaultOutbound();
        }
        if (rule.value("rule_set").toString() == ConfigConstants::RS_GEOSITE_GEOLOCATION_NOT_CN) {
          rule["outbound"] = settings.normalizedDefaultOutbound();
        }
        rules[i] = rule;
      }
      int hijackIndex = -1;
      for (int i = 0; i < rules.size(); ++i) {
        if (!rules[i].isObject()) continue;
        const QJsonObject rule = rules[i].toObject();
        if (rule.value("protocol").toString() == "dns" && rule.value("action").toString() == "hijack-dns") {
          hijackIndex = i;
          break;
        }
      }
      if (settings.dnsHijack()) {
        if (hijackIndex < 0) {
          QJsonObject hijackRule;
          hijackRule["protocol"] = "dns";
          hijackRule["action"]   = "hijack-dns";
          rules.insert(1, hijackRule);
        }
      } else if (hijackIndex >= 0) {
        rules.removeAt(hijackIndex);
      }
      int adsIndex = -1;
      for (int i = 0; i < rules.size(); ++i) {
        if (!rules[i].isObject()) continue;
        const QJsonObject rule = rules[i].toObject();
        if (rule.value("rule_set").toString() == ConfigConstants::RS_GEOSITE_ADS && rule.contains("action")) {
          adsIndex = i;
          break;
        }
      }
      if (settings.blockAds()) {
        if (adsIndex < 0) {
          QJsonObject adsRule;
          adsRule["rule_set"] = ConfigConstants::RS_GEOSITE_ADS;
          adsRule["action"]   = "reject";
          rules.append(adsRule);
        }
      } else if (adsIndex >= 0) {
        rules.removeAt(adsIndex);
      }
      if (!settings.enableAppGroups()) {
        QJsonArray filtered;
        for (const auto& ruleVal : rules) {
          if (!ruleVal.isObject()) {
            filtered.append(ruleVal);
            continue;
          }
          const QString rs = ruleVal.toObject().value("rule_set").toString();
          if (rs != ConfigConstants::RS_GEOSITE_TELEGRAM && rs != ConfigConstants::RS_GEOSITE_YOUTUBE &&
              rs != ConfigConstants::RS_GEOSITE_NETFLIX && rs != ConfigConstants::RS_GEOSITE_OPENAI) {
            filtered.append(ruleVal);
          }
        }
        rules = filtered;
      }
      route["rules"] = rules;
    }
    config["route"] = route;
  }
}

void ConfigMutator::applyPortSettings(QJsonObject& config) {
  const AppSettings& settings = AppSettings::instance();
  if (config.contains("experimental") && config["experimental"].isObject()) {
    QJsonObject experimental = config.value("experimental").toObject();
    if (experimental.contains("clash_api") && experimental["clash_api"].isObject()) {
      QJsonObject clashApi = experimental.value("clash_api").toObject();
      if (clashApi.contains("external_controller")) {
        clashApi["external_controller"] = QString("127.0.0.1:%1").arg(settings.apiPort());
      }
      experimental["clash_api"] = clashApi;
    }
    config["experimental"] = experimental;
  }
  if (config.contains("inbounds") && config["inbounds"].isArray()) {
    QJsonArray inbounds = config.value("inbounds").toArray();
    for (int i = 0; i < inbounds.size(); ++i) {
      if (!inbounds[i].isObject()) continue;
      QJsonObject   inbound = inbounds[i].toObject();
      const QString type    = inbound.value("type").toString();
      const QString tag     = inbound.value("tag").toString();
      if ((type == "mixed" || tag == "mixed-in") && inbound.contains("listen_port")) {
        inbound["listen_port"] = settings.mixedPort();
      }
      inbounds[i] = inbound;
    }
    config["inbounds"] = inbounds;
  }
}

bool ConfigMutator::updateClashDefaultMode(QJsonObject& config, const QString& mode, QString* error) {
  const QString normalized = mode.trimmed().toLower();
  if (normalized != "global" && normalized != "rule") {
    if (error) *error = QString("Invalid proxy mode: %1").arg(mode);
    return false;
  }
  QJsonObject experimental = config.value("experimental").toObject();
  QJsonObject clashApi     = experimental.value("clash_api").toObject();
  clashApi["default_mode"] = normalized;
  if (!clashApi.contains("external_ui")) {
    clashApi["external_ui"] = "metacubexd";
  }
  experimental["clash_api"] = clashApi;
  normalizeCacheFileConfig(experimental);
  config["experimental"] = experimental;
  return true;
}

QString ConfigMutator::readClashDefaultMode(const QJsonObject& config) {
  if (config.isEmpty()) {
    return "rule";
  }
  const QJsonObject experimental = config.value("experimental").toObject();
  const QJsonObject clashApi     = experimental.value("clash_api").toObject();
  const QString     mode         = clashApi.value("default_mode").toString().trimmed().toLower();
  if (mode == "global") {
    return "global";
  }
  return "rule";
}

void ConfigMutator::applySharedRules(QJsonObject& config, const QJsonArray& sharedRules, bool enabled) {
  if (!config.contains("route") || !config["route"].isObject()) {
    return;
  }
  QJsonObject route = config.value("route").toObject();
  QJsonArray  rules = route.value("rules").toArray();
  // helper: normalize rule for comparison (strip shared/source markers)
  auto normalize = [](QJsonObject obj) {
    obj.remove("shared");
    obj.remove("source");
    return obj;
  };
  // Remove existing rules that are identical to any shared rules (avoid duplication)
  QSet<QString> sharedSig;
  for (const auto& rv : sharedRules) {
    if (!rv.isObject()) continue;
    sharedSig.insert(QString::fromUtf8(QJsonDocument(normalize(rv.toObject())).toJson(QJsonDocument::Compact)));
  }
  if (!sharedSig.isEmpty()) {
    QJsonArray filtered;
    for (const auto& ruleVal : rules) {
      if (!ruleVal.isObject()) {
        filtered.append(ruleVal);
        continue;
      }
      const QString sig =
          QString::fromUtf8(QJsonDocument(normalize(ruleVal.toObject())).toJson(QJsonDocument::Compact));
      if (sharedSig.contains(sig)) {
        continue;  // drop old injected copy
      }
      filtered.append(ruleVal);
    }
    rules = filtered;
  }
  if (enabled && !sharedRules.isEmpty()) {
    int           insertIndex = findInsertIndex(rules);
    QSet<QString> dedup;
    for (const auto& ruleVal : sharedRules) {
      if (!ruleVal.isObject()) continue;
      QJsonObject   obj = normalize(ruleVal.toObject());
      const QString sig = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
      if (dedup.contains(sig)) continue;
      dedup.insert(sig);
      if (insertIndex < 0 || insertIndex > rules.size()) {
        rules.append(obj);
      } else {
        rules.insert(insertIndex, obj);
        insertIndex += 1;
      }
    }
  }
  route["rules"]  = rules;
  config["route"] = route;
}
