#include "SubscriptionService.h"
#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include "app/interfaces/ConfigRepository.h"
#include "services/config/ConfigMutator.h"
#include "services/rules/SharedRulesStore.h"
#include "services/subscription/SubscriptionParser.h"
#include "storage/ConfigConstants.h"
#include "storage/DatabaseService.h"
#include "storage/SubscriptionConfigStore.h"
#include "utils/Logger.h"
#include "utils/subscription/SubscriptionUserinfo.h"

namespace {
constexpr qint64 kUnsetValue = -1;

QJsonValue canonicalJsonValue(const QJsonValue& value) {
  if (value.isObject()) {
    const QJsonObject source = value.toObject();
    const QStringList keys   = source.keys();
    QJsonObject       sorted;
    for (const auto& key : keys) {
      sorted.insert(key, canonicalJsonValue(source.value(key)));
    }
    return sorted;
  }
  if (value.isArray()) {
    QJsonArray sorted;
    const auto arr = value.toArray();
    for (const auto& item : arr) {
      sorted.append(canonicalJsonValue(item));
    }
    return sorted;
  }
  return value;
}

QString nodeSignature(const QJsonObject& node) {
  QJsonObject normalized = node;
  normalized.remove("tag");
  normalized.remove("domain_resolver");
  const QJsonObject canonical = canonicalJsonValue(normalized).toObject();
  return QString::fromUtf8(
      QJsonDocument(canonical).toJson(QJsonDocument::Compact));
}

bool isSelectorLike(const QJsonObject& outbound) {
  const QString type = outbound.value("type").toString().trimmed().toLower();
  if (type == "selector" || type == "urltest" || type == "fallback" ||
      type == "direct" || type == "block" || type == "dns") {
    return true;
  }
  return outbound.contains("outbounds") &&
         outbound.value("outbounds").isArray();
}

bool isNodeOutboundCandidate(const QJsonObject& outbound) {
  if (outbound.value("tag").toString().trimmed().isEmpty()) {
    return false;
  }
  if (outbound.value("type").toString().trimmed().isEmpty()) {
    return false;
  }
  return !isSelectorLike(outbound);
}

QString fallbackNodeTag(const QJsonObject& node, int index) {
  QString tag = node.value("name").toString().trimmed();
  if (!tag.isEmpty()) {
    return tag;
  }
  const QString server = node.value("server").toString().trimmed();
  const int     port   = node.value("server_port").toInt();
  if (!server.isEmpty() && port > 0) {
    return QString("%1:%2").arg(server).arg(port);
  }
  QString type = node.value("type").toString().trimmed();
  if (type.isEmpty()) {
    type = QStringLiteral("node");
  }
  return QString("%1-%2").arg(type).arg(index + 1);
}

QString makeUniqueTag(const QString& preferred, const QSet<QString>& existing) {
  QString base = preferred.trimmed();
  if (base.isEmpty()) {
    base = QStringLiteral("node");
  }
  if (!existing.contains(base)) {
    return base;
  }
  int counter = 2;
  while (true) {
    const QString candidate = QString("%1-%2").arg(base).arg(counter);
    if (!existing.contains(candidate)) {
      return candidate;
    }
    ++counter;
  }
}

QJsonArray loadSourceNodes(const SubscriptionInfo& source,
                           ConfigRepository*       cfgRepo) {
  QJsonArray nodes = DatabaseService::instance().getSubscriptionNodes(source.id);
  if (nodes.isEmpty() && source.isManual && !source.useOriginalConfig &&
      !source.manualContent.trimmed().isEmpty()) {
    nodes = SubscriptionParser::extractNodesWithFallback(source.manualContent);
  }
  if (!nodes.isEmpty() || !cfgRepo || source.configPath.trimmed().isEmpty()) {
    return nodes;
  }
  const QJsonObject sourceConfig = cfgRepo->loadConfig(source.configPath);
  const QJsonArray  outbounds    = sourceConfig.value("outbounds").toArray();
  for (const auto& item : outbounds) {
    if (!item.isObject()) {
      continue;
    }
    const QJsonObject outbound = item.toObject();
    const QString     tag      = outbound.value("tag").toString().trimmed();
    if (tag.isEmpty()) {
      continue;
    }
    if (tag == ConfigConstants::TAG_AUTO || tag == ConfigConstants::TAG_MANUAL ||
        tag == ConfigConstants::TAG_DIRECT || tag == ConfigConstants::TAG_BLOCK ||
        tag == ConfigConstants::TAG_TELEGRAM ||
        tag == ConfigConstants::TAG_YOUTUBE ||
        tag == ConfigConstants::TAG_NETFLIX ||
        tag == ConfigConstants::TAG_OPENAI) {
      continue;
    }
    if (isNodeOutboundCandidate(outbound)) {
      nodes.append(outbound);
    }
  }
  return nodes;
}

QString resolveImportGroupName(const SubscriptionInfo& source,
                               const QJsonArray&      sourceNodes) {
  if (source.isManual && sourceNodes.size() == 1 && sourceNodes.first().isObject()) {
    const QString nodeTag =
        sourceNodes.first().toObject().value("tag").toString().trimmed();
    if (!nodeTag.isEmpty()) {
      return nodeTag;
    }
  }
  const QString subscriptionName = source.name.trimmed();
  if (!subscriptionName.isEmpty()) {
    return subscriptionName;
  }
  return source.isManual ? QObject::tr("Manual Node")
                         : QObject::tr("Imported Group");
}

bool isReservedImportGroupTag(const QString& tag) {
  const QString normalized = tag.trimmed();
  return normalized == ConfigConstants::TAG_AUTO ||
         normalized == ConfigConstants::TAG_MANUAL ||
         normalized == ConfigConstants::TAG_DIRECT ||
         normalized == ConfigConstants::TAG_BLOCK ||
         normalized == ConfigConstants::TAG_TELEGRAM ||
         normalized == ConfigConstants::TAG_YOUTUBE ||
         normalized == ConfigConstants::TAG_NETFLIX ||
         normalized == ConfigConstants::TAG_OPENAI ||
         normalized.compare("GLOBAL", Qt::CaseInsensitive) == 0;
}
}  // namespace

SubscriptionService::SubscriptionService(ConfigRepository* configRepo,
                                         QObject*          parent)
    : QObject(parent), m_activeIndex(-1), m_configRepo(configRepo) {
  QJsonArray subs = DatabaseService::instance().getSubscriptions();
  for (const auto& val : subs) {
    QJsonObject      obj = val.toObject();
    SubscriptionInfo info;
    info.id         = obj.value("id").toString();
    info.name       = obj.value("name").toString();
    info.url        = obj.value("url").toString();
    info.lastUpdate = obj.value("last_update").toVariant().toLongLong();
    if (info.lastUpdate <= 0 && obj.value("last_update").isString()) {
      info.lastUpdate = QDateTime::fromString(
                            obj.value("last_update").toString(), Qt::ISODate)
                            .toMSecsSinceEpoch();
    }
    info.nodeCount         = obj.value("node_count").toInt();
    info.enabled           = obj.value("enabled").toBool(true);
    info.isManual          = obj.value("is_manual").toBool(false);
    info.manualContent     = obj.value("manual_content").toString();
    info.useOriginalConfig = obj.value("use_original_config").toBool(false);
    info.configPath        = obj.value("config_path").toString();
    info.backupPath        = obj.value("backup_path").toString();
    info.autoUpdateIntervalMinutes =
        obj.value("auto_update_interval_minutes").toInt(720);
    info.subscriptionUpload =
        obj.value("subscription_upload").toVariant().toLongLong();
    info.subscriptionDownload =
        obj.value("subscription_download").toVariant().toLongLong();
    info.subscriptionTotal =
        obj.value("subscription_total").toVariant().toLongLong();
    info.subscriptionExpire =
        obj.value("subscription_expire").toVariant().toLongLong();
    info.enableSharedRules = obj.value("enable_shared_rules").toBool(true);
    if (obj.contains("rule_sets") && obj.value("rule_sets").isArray()) {
      const QJsonArray arr = obj.value("rule_sets").toArray();
      for (const auto& v : arr) {
        const QString name = v.toString().trimmed();
        if (!name.isEmpty()) {
          info.ruleSets.append(name);
        }
      }
    }
    if (!obj.contains("subscription_upload")) {
      info.subscriptionUpload = kUnsetValue;
    }
    if (!obj.contains("subscription_download")) {
      info.subscriptionDownload = kUnsetValue;
    }
    if (!obj.contains("subscription_total")) {
      info.subscriptionTotal = kUnsetValue;
    }
    if (!obj.contains("subscription_expire")) {
      info.subscriptionExpire = kUnsetValue;
    }
    if (info.ruleSets.isEmpty()) {
      info.ruleSets << "default";
    }
    m_subscriptions.append(info);
  }
  m_activeIndex      = DatabaseService::instance().getActiveSubscriptionIndex();
  m_activeConfigPath = DatabaseService::instance().getActiveConfigPath();
  if (m_activeIndex >= 0 && m_activeIndex < m_subscriptions.count()) {
    syncSharedRulesToConfig(m_subscriptions[m_activeIndex]);
  }
}

SubscriptionService::~SubscriptionService() = default;

void SubscriptionService::saveToDatabase() {
  QJsonArray array;
  for (const auto& info : m_subscriptions) {
    QJsonObject obj;
    obj["id"]                           = info.id;
    obj["name"]                         = info.name;
    obj["url"]                          = info.url;
    obj["last_update"]                  = info.lastUpdate;
    obj["node_count"]                   = info.nodeCount;
    obj["enabled"]                      = info.enabled;
    obj["is_manual"]                    = info.isManual;
    obj["manual_content"]               = info.manualContent;
    obj["use_original_config"]          = info.useOriginalConfig;
    obj["config_path"]                  = info.configPath;
    obj["backup_path"]                  = info.backupPath;
    obj["auto_update_interval_minutes"] = info.autoUpdateIntervalMinutes;
    if (info.subscriptionUpload >= 0) {
      obj["subscription_upload"] = info.subscriptionUpload;
    }
    if (info.subscriptionDownload >= 0) {
      obj["subscription_download"] = info.subscriptionDownload;
    }
    if (info.subscriptionTotal >= 0) {
      obj["subscription_total"] = info.subscriptionTotal;
    }
    if (info.subscriptionExpire >= 0) {
      obj["subscription_expire"] = info.subscriptionExpire;
    }
    obj["enable_shared_rules"] = info.enableSharedRules;
    QJsonArray rs;
    for (const auto& name : info.ruleSets) {
      if (!name.trimmed().isEmpty()) {
        rs.append(name.trimmed());
      }
    }
    obj["rule_sets"] = rs;
    array.append(obj);
  }
  DatabaseService::instance().saveSubscriptions(array);
  DatabaseService::instance().saveActiveSubscriptionIndex(m_activeIndex);
  DatabaseService::instance().saveActiveConfigPath(m_activeConfigPath);
}

SubscriptionInfo* SubscriptionService::findSubscription(const QString& id) {
  for (auto& sub : m_subscriptions) {
    if (sub.id == id) {
      return &sub;
    }
  }
  return nullptr;
}

bool SubscriptionService::isJsonContent(const QString& content) const {
  QJsonParseError err;
  QJsonDocument   doc = QJsonDocument::fromJson(content.toUtf8(), &err);
  return err.error == QJsonParseError::NoError && doc.isObject();
}

void SubscriptionService::updateSubscriptionUserinfo(
    SubscriptionInfo& info, const QJsonObject& headers) {
  if (headers.isEmpty()) {
    info.subscriptionUpload   = kUnsetValue;
    info.subscriptionDownload = kUnsetValue;
    info.subscriptionTotal    = kUnsetValue;
    info.subscriptionExpire   = kUnsetValue;
    return;
  }
  info.subscriptionUpload = headers.value("upload").toVariant().toLongLong();
  info.subscriptionDownload =
      headers.value("download").toVariant().toLongLong();
  info.subscriptionTotal  = headers.value("total").toVariant().toLongLong();
  info.subscriptionExpire = headers.value("expire").toVariant().toLongLong();
}

void SubscriptionService::updateSubscriptionUserinfoFromHeader(
    SubscriptionInfo& info, const QByteArray& header) {
  updateSubscriptionUserinfo(info,
                             SubscriptionUserinfo::parseUserinfoHeader(header));
}

void SubscriptionService::syncSharedRulesToConfig(
    const SubscriptionInfo& info, const QStringList& cleanupRuleSets) {
  if (info.configPath.isEmpty()) {
    return;
  }
  if (!m_configRepo) {
    return;
  }
  QJsonObject config = m_configRepo->loadConfig(info.configPath);
  if (config.isEmpty()) {
    return;
  }
  auto normalizeSetNames = [](const QStringList& setNames) {
    QStringList normalized;
    for (const auto& rawName : setNames) {
      const QString name = rawName.trimmed();
      if (!name.isEmpty()) {
        normalized.append(name);
      }
    }
    normalized.removeDuplicates();
    if (normalized.isEmpty()) {
      normalized.append(QStringLiteral("default"));
    }
    return normalized;
  };
  auto collectRules = [](const QStringList& setNames) {
    QJsonArray   merged;
    QSet<QString> signatures;
    for (const auto& name : setNames) {
      const QJsonArray rules = SharedRulesStore::loadRules(name);
      for (const auto& rule : rules) {
        if (!rule.isObject()) {
          continue;
        }
        const QString sig =
            QString::fromUtf8(QJsonDocument(rule.toObject())
                                  .toJson(QJsonDocument::Compact));
        if (signatures.contains(sig)) {
          continue;
        }
        signatures.insert(sig);
        merged.append(rule);
      }
    }
    return merged;
  };
  const QStringList selectedSetNames = normalizeSetNames(info.ruleSets);
  const QStringList cleanupSetNames  = cleanupRuleSets.isEmpty()
                                           ? selectedSetNames
                                           : normalizeSetNames(cleanupRuleSets);
  const QJsonArray cleanupRules = collectRules(cleanupSetNames);
  if (!cleanupRules.isEmpty()) {
    ConfigMutator::applySharedRules(config, cleanupRules, false);
  }
  if (info.enableSharedRules) {
    const QJsonArray selectedRules = collectRules(selectedSetNames);
    if (!selectedRules.isEmpty()) {
      ConfigMutator::applySharedRules(config, selectedRules, true);
    }
  }
  m_configRepo->saveConfig(info.configPath, config);
}

void SubscriptionService::addUrlSubscription(const QString& url,
                                             const QString& name,
                                             bool           useOriginalConfig,
                                             int  autoUpdateIntervalMinutes,
                                             bool applyRuntime,
                                             bool enableSharedRules,
                                             const QStringList& ruleSets) {
  const QString trimmedUrl = url.trimmed();
  if (trimmedUrl.isEmpty()) {
    emit errorOccurred(tr("Please enter a subscription URL"));
    return;
  }
  QString subName =
      name.trimmed().isEmpty() ? QUrl(trimmedUrl).host() : name.trimmed();
  QString       id = generateId();
  const QString configName =
      SubscriptionConfigStore::generateConfigFileName(subName);
  const QString configPath =
      m_configRepo ? m_configRepo->getConfigDir() + "/" + configName
                   : QString();
  if (configPath.isEmpty()) {
    emit errorOccurred(tr("Config directory not available"));
    return;
  }
  Logger::info(QString("Add subscription: %1 (%2)").arg(subName, trimmedUrl));
  QNetworkRequest request{QUrl(trimmedUrl)};
  request.setTransferTimeout(30000);
  QNetworkAccessManager* manager = new QNetworkAccessManager(this);
  QNetworkReply*         reply   = manager->get(request);
  connect(reply, &QNetworkReply::finished, this, [=]() {
    reply->deleteLater();
    manager->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      emit errorOccurred(tr("Failed to fetch subscription"));
      return;
    }
    const QByteArray data           = reply->readAll();
    const QByteArray userinfoHeader = reply->rawHeader("subscription-userinfo");
    SubscriptionInfo info;
    info.id       = id;
    info.name     = subName;
    info.url      = trimmedUrl;
    info.enabled  = true;
    info.isManual = false;
    info.manualContent.clear();
    info.useOriginalConfig         = useOriginalConfig;
    info.autoUpdateIntervalMinutes = autoUpdateIntervalMinutes;
    info.configPath                = configPath;
    info.backupPath                = configPath + ".bak";
    info.enableSharedRules         = enableSharedRules;
    info.ruleSets = ruleSets.isEmpty() ? QStringList{"default"} : ruleSets;
    bool saved    = false;
    if (useOriginalConfig) {
      if (!isJsonContent(QString::fromUtf8(data))) {
        emit errorOccurred(
            tr("Original subscription only supports sing-box JSON config"));
        return;
      }
      saved = SubscriptionConfigStore::saveOriginalConfig(
          m_configRepo, QString::fromUtf8(data), configPath);
    } else {
      QJsonArray nodes =
          SubscriptionParser::extractNodesWithFallback(QString::fromUtf8(data));
      if (nodes.isEmpty() && isJsonContent(QString::fromUtf8(data))) {
        // Fall back to saving original config when JSON is a full config.
        info.useOriginalConfig = true;
        saved                  = SubscriptionConfigStore::saveOriginalConfig(
            m_configRepo, QString::fromUtf8(data), configPath);
      } else {
        if (nodes.isEmpty()) {
          emit errorOccurred(
              tr("Failed to extract nodes from subscription content; check "
                 "format"));
          return;
        }
        info.nodeCount = nodes.count();
        saved          = SubscriptionConfigStore::saveConfigWithNodes(
            m_configRepo, nodes, configPath);
        DatabaseService::instance().saveSubscriptionNodes(id, nodes);
      }
    }
    if (!saved) {
      emit errorOccurred(tr("Failed to save subscription config"));
      return;
    }
    info.lastUpdate = QDateTime::currentMSecsSinceEpoch();
    updateSubscriptionUserinfoFromHeader(info, userinfoHeader);
    syncSharedRulesToConfig(info);
    m_subscriptions.append(info);
    m_activeIndex      = m_subscriptions.count() - 1;
    m_activeConfigPath = configPath;
    saveToDatabase();
    emit subscriptionAdded(info);
    emit activeSubscriptionChanged(info.id, info.configPath);
    if (applyRuntime) {
      emit applyConfigRequested(info.configPath, true);
    }
  });
}

void SubscriptionService::addManualSubscription(const QString& content,
                                                const QString& name,
                                                bool useOriginalConfig,
                                                bool isUriList,
                                                bool applyRuntime,
                                                bool enableSharedRules,
                                                const QStringList& ruleSets) {
  Q_UNUSED(isUriList)
  const QString trimmed = content.trimmed();
  if (trimmed.isEmpty()) {
    emit errorOccurred(tr("Please enter subscription content"));
    return;
  }
  if (useOriginalConfig && !isJsonContent(trimmed)) {
    emit errorOccurred(
        tr("Original subscription only supports sing-box JSON config"));
    return;
  }
  QString subName =
      name.trimmed().isEmpty() ? tr("Manual subscription") : name.trimmed();
  QString       id = generateId();
  const QString configName =
      SubscriptionConfigStore::generateConfigFileName(subName);
  const QString configPath =
      m_configRepo ? m_configRepo->getConfigDir() + "/" + configName
                   : QString();
  if (configPath.isEmpty()) {
    emit errorOccurred(tr("Config directory not available"));
    return;
  }
  SubscriptionInfo info;
  info.id   = id;
  info.name = subName;
  info.url.clear();
  info.enabled                   = true;
  info.isManual                  = true;
  info.manualContent             = trimmed;
  info.useOriginalConfig         = useOriginalConfig;
  info.autoUpdateIntervalMinutes = 0;
  info.configPath                = configPath;
  info.backupPath                = configPath + ".bak";
  info.enableSharedRules         = enableSharedRules;
  info.ruleSets = ruleSets.isEmpty() ? QStringList{"default"} : ruleSets;
  bool saved    = false;
  if (useOriginalConfig) {
    saved = SubscriptionConfigStore::saveOriginalConfig(
        m_configRepo, trimmed, configPath);
  } else {
    QJsonArray nodes = SubscriptionParser::extractNodesWithFallback(trimmed);
    if (nodes.isEmpty() && isJsonContent(trimmed)) {
      info.useOriginalConfig = true;
      saved                  = SubscriptionConfigStore::saveOriginalConfig(
          m_configRepo, trimmed, configPath);
    } else {
      if (nodes.isEmpty()) {
        emit errorOccurred(tr(
            "Failed to extract nodes from subscription content; check format"));
        return;
      }
      info.nodeCount = nodes.count();
      saved          = SubscriptionConfigStore::saveConfigWithNodes(
          m_configRepo, nodes, configPath);
      DatabaseService::instance().saveSubscriptionNodes(id, nodes);
    }
  }
  if (!saved) {
    emit errorOccurred(tr("Failed to save subscription config"));
    return;
  }
  info.lastUpdate           = QDateTime::currentMSecsSinceEpoch();
  info.subscriptionUpload   = kUnsetValue;
  info.subscriptionDownload = kUnsetValue;
  info.subscriptionTotal    = kUnsetValue;
  info.subscriptionExpire   = kUnsetValue;
  syncSharedRulesToConfig(info);
  m_subscriptions.append(info);
  m_activeIndex      = m_subscriptions.count() - 1;
  m_activeConfigPath = configPath;
  saveToDatabase();
  emit subscriptionAdded(info);
  emit activeSubscriptionChanged(info.id, info.configPath);
  if (applyRuntime) {
    emit applyConfigRequested(info.configPath, true);
  }
}

void SubscriptionService::removeSubscription(const QString& id) {
  for (int i = 0; i < m_subscriptions.count(); ++i) {
    if (m_subscriptions[i].id == id) {
      const QString removedConfig = m_subscriptions[i].configPath;
      m_subscriptions.removeAt(i);
      if (m_activeIndex == i) {
        m_activeIndex = -1;
        m_activeConfigPath.clear();
      } else if (m_activeIndex > i) {
        m_activeIndex -= 1;
      }
      saveToDatabase();
      emit subscriptionRemoved(id);
      if (!removedConfig.isEmpty()) {
        deleteSubscriptionConfig(removedConfig);
      }
      return;
    }
  }
}

void SubscriptionService::refreshSubscription(const QString& id,
                                              bool           applyRuntime) {
  SubscriptionInfo* sub = findSubscription(id);
  if (!sub) {
    emit errorOccurred(tr("Subscription not found"));
    return;
  }
  if (sub->isManual) {
    if (sub->manualContent.trimmed().isEmpty()) {
      emit errorOccurred(tr("Manual subscription content is empty"));
      return;
    }
    if (sub->useOriginalConfig && !isJsonContent(sub->manualContent)) {
      emit errorOccurred(
          tr("Original subscription only supports sing-box JSON config"));
      return;
    }
    bool saved = false;
    if (sub->useOriginalConfig) {
      saved = SubscriptionConfigStore::saveOriginalConfig(
          m_configRepo, sub->manualContent, sub->configPath);
    } else {
      QJsonArray nodes =
          SubscriptionParser::extractNodesWithFallback(sub->manualContent);
      if (nodes.isEmpty() && isJsonContent(sub->manualContent)) {
        sub->useOriginalConfig = true;
        saved                  = SubscriptionConfigStore::saveOriginalConfig(
            m_configRepo, sub->manualContent, sub->configPath);
      } else {
        if (nodes.isEmpty()) {
          emit errorOccurred(
              tr("Failed to extract nodes from subscription content; check "
                 "format"));
          return;
        }
        sub->nodeCount = nodes.count();
        saved          = SubscriptionConfigStore::saveConfigWithNodes(
            m_configRepo, nodes, sub->configPath);
        DatabaseService::instance().saveSubscriptionNodes(sub->id, nodes);
      }
    }
    if (!saved) {
      emit errorOccurred(tr("Failed to refresh subscription"));
      return;
    }
    sub->lastUpdate           = QDateTime::currentMSecsSinceEpoch();
    sub->subscriptionUpload   = kUnsetValue;
    sub->subscriptionDownload = kUnsetValue;
    sub->subscriptionTotal    = kUnsetValue;
    sub->subscriptionExpire   = kUnsetValue;
    syncSharedRulesToConfig(*sub);
    saveToDatabase();
    emit subscriptionUpdated(sub->id);
    if (applyRuntime) {
      emit applyConfigRequested(sub->configPath, true);
    }
    return;
  }
  const QString url = sub->url.trimmed();
  if (url.isEmpty()) {
    emit errorOccurred(tr("Subscription URL is empty"));
    return;
  }
  QNetworkRequest request{QUrl(url)};
  request.setTransferTimeout(30000);
  QNetworkAccessManager* manager = new QNetworkAccessManager(this);
  QNetworkReply*         reply   = manager->get(request);
  connect(reply, &QNetworkReply::finished, this, [=]() {
    reply->deleteLater();
    manager->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      emit errorOccurred(tr("Failed to update subscription"));
      return;
    }
    const QByteArray data           = reply->readAll();
    const QByteArray userinfoHeader = reply->rawHeader("subscription-userinfo");
    bool             saved          = false;
    if (sub->useOriginalConfig) {
      if (!isJsonContent(QString::fromUtf8(data))) {
        emit errorOccurred(
            tr("Original subscription only supports sing-box JSON config"));
        return;
      }
      saved = SubscriptionConfigStore::saveOriginalConfig(
          m_configRepo, QString::fromUtf8(data), sub->configPath);
    } else {
      QJsonArray nodes =
          SubscriptionParser::extractNodesWithFallback(QString::fromUtf8(data));
      if (nodes.isEmpty() && isJsonContent(QString::fromUtf8(data))) {
        sub->useOriginalConfig = true;
        saved                  = SubscriptionConfigStore::saveOriginalConfig(
            m_configRepo, QString::fromUtf8(data), sub->configPath);
      } else {
        if (nodes.isEmpty()) {
          emit errorOccurred(
              tr("Failed to extract nodes from subscription content; check "
                 "format"));
          return;
        }
        sub->nodeCount = nodes.count();
        saved          = SubscriptionConfigStore::saveConfigWithNodes(
            m_configRepo, nodes, sub->configPath);
        DatabaseService::instance().saveSubscriptionNodes(sub->id, nodes);
      }
    }
    if (!saved) {
      emit errorOccurred(tr("Failed to save subscription config"));
      return;
    }
    sub->lastUpdate = QDateTime::currentMSecsSinceEpoch();
    updateSubscriptionUserinfoFromHeader(*sub, userinfoHeader);
    syncSharedRulesToConfig(*sub);
    saveToDatabase();
    emit subscriptionUpdated(sub->id);
    if (applyRuntime) {
      emit applyConfigRequested(sub->configPath, true);
    }
  });
}

void SubscriptionService::updateAllSubscriptions(bool applyRuntime) {
  for (const auto& sub : m_subscriptions) {
    if (sub.enabled) {
      refreshSubscription(sub.id, applyRuntime);
    }
  }
}

void SubscriptionService::updateSubscriptionMeta(const QString& id,
                                                 const QString& name,
                                                 const QString& url,
                                                 bool           isManual,
                                                 const QString& manualContent,
                                                 bool useOriginalConfig,
                                                 int  autoUpdateIntervalMinutes,
                                                 bool enableSharedRules,
                                                 const QStringList& ruleSets) {
  SubscriptionInfo* sub = findSubscription(id);
  if (!sub) {
    emit errorOccurred(tr("Subscription not found"));
    return;
  }
  const QStringList oldRuleSets          = sub->ruleSets;
  const bool        oldEnableSharedRules = sub->enableSharedRules;
  sub->name                               = name.trimmed();
  sub->url                                = url.trimmed();
  sub->isManual                           = isManual;
  sub->manualContent                      = manualContent;
  sub->useOriginalConfig                  = useOriginalConfig;
  sub->autoUpdateIntervalMinutes          = autoUpdateIntervalMinutes;
  sub->enableSharedRules                  = enableSharedRules;
  sub->ruleSets = ruleSets.isEmpty() ? QStringList{"default"} : ruleSets;
  QStringList cleanupRuleSets = oldRuleSets;
  for (const auto& setName : sub->ruleSets) {
    if (!cleanupRuleSets.contains(setName)) {
      cleanupRuleSets.append(setName);
    }
  }
  saveToDatabase();
  syncSharedRulesToConfig(*sub, cleanupRuleSets);
  emit subscriptionUpdated(id);
  const bool sharedRulesChanged = sub->ruleSets != oldRuleSets ||
                                  sub->enableSharedRules != oldEnableSharedRules;
  const bool isActiveSubscription = m_activeIndex >= 0 &&
                                    m_activeIndex < m_subscriptions.count() &&
                                    m_subscriptions[m_activeIndex].id == sub->id;
  if (sharedRulesChanged && isActiveSubscription && !sub->configPath.isEmpty()) {
    emit applyConfigRequested(sub->configPath, true);
  }
}

void SubscriptionService::setActiveSubscription(const QString& id,
                                                bool           applyRuntime) {
  for (int i = 0; i < m_subscriptions.count(); ++i) {
    if (m_subscriptions[i].id == id) {
      m_activeIndex      = i;
      m_activeConfigPath = m_subscriptions[i].configPath;
      syncSharedRulesToConfig(m_subscriptions[i]);
      saveToDatabase();
      emit activeSubscriptionChanged(id, m_activeConfigPath);
      if (applyRuntime && !m_activeConfigPath.isEmpty()) {
        emit applyConfigRequested(m_activeConfigPath, true);
      }
      return;
    }
  }
  emit errorOccurred(tr("Subscription not found"));
}

void SubscriptionService::clearActiveSubscription() {
  m_activeIndex = -1;
  m_activeConfigPath.clear();
  saveToDatabase();
  emit activeSubscriptionChanged(QString(), QString());
}

bool SubscriptionService::addSubscriptionNodesToActiveGroup(
    const QString& sourceId, QString* error) {
  auto setError = [error](const QString& message) {
    if (error) {
      *error = message;
    }
  };

  const QString id = sourceId.trimmed();
  if (id.isEmpty()) {
    setError(tr("Subscription not found."));
    return false;
  }
  SubscriptionInfo* source = findSubscription(id);
  if (!source) {
    setError(tr("Subscription not found."));
    return false;
  }
  if (m_activeIndex < 0 || m_activeIndex >= m_subscriptions.size()) {
    setError(tr("Active subscription not found."));
    return false;
  }
  SubscriptionInfo& active = m_subscriptions[m_activeIndex];
  if (active.id == source->id) {
    setError(tr("Cannot import from active subscription itself."));
    return false;
  }
  if (!m_configRepo) {
    setError(tr("Config service not available."));
    return false;
  }
  if (active.configPath.trimmed().isEmpty()) {
    setError(tr("Active config not found."));
    return false;
  }

  const QJsonArray sourceNodes = loadSourceNodes(*source, m_configRepo);
  if (sourceNodes.isEmpty()) {
    setError(tr("No node data available for this subscription."));
    return false;
  }

  QJsonObject config = m_configRepo->loadConfig(active.configPath);
  if (config.isEmpty()) {
    setError(tr("Failed to read config file: %1").arg(active.configPath));
    return false;
  }
  QJsonArray outbounds = config.value("outbounds").toArray();
  if (outbounds.isEmpty()) {
    setError(tr("Current config does not contain outbounds."));
    return false;
  }

  QSet<QString>             existingTags;
  QHash<QString, QString>   signatureToTag;
  auto registerOutbound = [&](const QJsonObject& outbound) {
    const QString tag = outbound.value("tag").toString().trimmed();
    if (!tag.isEmpty()) {
      existingTags.insert(tag);
    }
    if (!isNodeOutboundCandidate(outbound)) {
      return;
    }
    const QString signature = nodeSignature(outbound);
    if (!signature.isEmpty() && !tag.isEmpty()) {
      signatureToTag.insert(signature, tag);
    }
  };
  for (const auto& item : outbounds) {
    if (!item.isObject()) {
      continue;
    }
    registerOutbound(item.toObject());
  }

  QStringList groupNodeTags;
  QJsonArray  newNodes;
  int         fallbackIdx = 0;
  for (const auto& item : sourceNodes) {
    if (!item.isObject()) {
      continue;
    }
    QJsonObject node = item.toObject();
    if (node.value("type").toString().trimmed().isEmpty()) {
      continue;
    }
    QString rawTag = node.value("tag").toString().trimmed();
    if (rawTag.isEmpty()) {
      rawTag = fallbackNodeTag(node, fallbackIdx);
    }
    ++fallbackIdx;
    const QString signature = nodeSignature(node);
    if (!signature.isEmpty() && signatureToTag.contains(signature)) {
      const QString existingTag = signatureToTag.value(signature).trimmed();
      if (!existingTag.isEmpty()) {
        groupNodeTags.append(existingTag);
      }
      continue;
    }
    const QString uniqueTag = makeUniqueTag(rawTag, existingTags);
    node["tag"]             = uniqueTag;
    existingTags.insert(uniqueTag);
    const QString uniqueSignature = nodeSignature(node);
    if (!uniqueSignature.isEmpty()) {
      signatureToTag.insert(uniqueSignature, uniqueTag);
    }
    groupNodeTags.append(uniqueTag);
    newNodes.append(node);
  }
  groupNodeTags.removeDuplicates();
  if (groupNodeTags.isEmpty()) {
    setError(tr("No valid nodes can be imported."));
    return false;
  }

  QString desiredGroupTag = resolveImportGroupName(*source, sourceNodes);
  if (isReservedImportGroupTag(desiredGroupTag)) {
    desiredGroupTag += "-import";
  }
  QString finalGroupTag = desiredGroupTag;
  int           groupIndex      = -1;
  for (int i = 0; i < outbounds.size(); ++i) {
    if (!outbounds[i].isObject()) {
      continue;
    }
    const QJsonObject outbound = outbounds[i].toObject();
    if (outbound.value("tag").toString() != desiredGroupTag) {
      continue;
    }
    if (outbound.value("type").toString() == "selector") {
      groupIndex = i;
      break;
    }
  }
  if (groupIndex < 0 && existingTags.contains(finalGroupTag)) {
    finalGroupTag = makeUniqueTag(finalGroupTag, existingTags);
  }

  for (const auto& node : newNodes) {
    outbounds.append(node);
  }

  if (groupIndex >= 0) {
    QJsonObject groupObj = outbounds[groupIndex].toObject();
    QStringList members;
    const QJsonArray current = groupObj.value("outbounds").toArray();
    for (const auto& item : current) {
      const QString member = item.toString().trimmed();
      if (!member.isEmpty()) {
        members.append(member);
      }
    }
    for (const auto& tag : groupNodeTags) {
      if (!members.contains(tag)) {
        members.append(tag);
      }
    }
    groupObj["outbounds"] = QJsonArray::fromStringList(members);
    outbounds[groupIndex] = groupObj;
  } else {
    QJsonObject groupObj;
    groupObj["type"]      = "selector";
    groupObj["tag"]       = finalGroupTag;
    groupObj["outbounds"] = QJsonArray::fromStringList(groupNodeTags);
    outbounds.append(groupObj);
    existingTags.insert(finalGroupTag);
  }

  // Keep imported selector reachable from manual chain so Clash /proxies can
  // discover it in runtime.
  for (int i = 0; i < outbounds.size(); ++i) {
    if (!outbounds[i].isObject()) {
      continue;
    }
    QJsonObject ob = outbounds[i].toObject();
    if (ob.value("tag").toString() != ConfigConstants::TAG_MANUAL ||
        ob.value("type").toString() != "selector") {
      continue;
    }
    QStringList members;
    const QJsonArray current = ob.value("outbounds").toArray();
    for (const auto& item : current) {
      const QString member = item.toString().trimmed();
      if (!member.isEmpty()) {
        members.append(member);
      }
    }
    if (!members.contains(finalGroupTag)) {
      members.append(finalGroupTag);
      ob["outbounds"] = QJsonArray::fromStringList(members);
      outbounds[i]    = ob;
    }
    break;
  }

  config["outbounds"] = outbounds;
  if (!m_configRepo->saveConfig(active.configPath, config)) {
    setError(tr("Failed to save config: %1").arg(active.configPath));
    return false;
  }
  emit applyConfigRequested(active.configPath, true);
  return true;
}

bool SubscriptionService::removeGroupFromActiveConfig(const QString& groupTag,
                                                       QString*       error) {
  auto setError = [this, error](const QString& msg) {
    if (error) {
      *error = msg;
    }
    emit errorOccurred(msg);
  };
  if (groupTag.isEmpty()) {
    setError(tr("Group tag is empty."));
    return false;
  }
  if (isReservedImportGroupTag(groupTag)) {
    setError(tr("Cannot delete a built-in group."));
    return false;
  }
  if (m_activeIndex < 0 || m_activeIndex >= m_subscriptions.size()) {
    setError(tr("No active subscription."));
    return false;
  }
  const SubscriptionInfo& active = m_subscriptions[m_activeIndex];
  if (active.configPath.isEmpty() || !m_configRepo) {
    setError(tr("Active subscription has no config path."));
    return false;
  }
  QJsonObject config = m_configRepo->loadConfig(active.configPath);
  if (config.isEmpty()) {
    setError(tr("Failed to load config: %1").arg(active.configPath));
    return false;
  }
  QJsonArray outbounds = config.value("outbounds").toArray();

  // Find the group and collect its member tags.
  int         groupIndex = -1;
  QStringList memberTags;
  for (int i = 0; i < outbounds.size(); ++i) {
    if (!outbounds[i].isObject()) {
      continue;
    }
    const QJsonObject ob = outbounds[i].toObject();
    if (ob.value("tag").toString() == groupTag) {
      groupIndex = i;
      const QJsonArray members = ob.value("outbounds").toArray();
      for (const auto& m : members) {
        const QString tag = m.toString().trimmed();
        if (!tag.isEmpty()) {
          memberTags.append(tag);
        }
      }
      break;
    }
  }
  if (groupIndex < 0) {
    setError(tr("Group '%1' not found in config.").arg(groupTag));
    return false;
  }

  // Find which member tags are exclusive to this group (not referenced by
  // any other selector/urltest/fallback group).
  QSet<QString> exclusiveTags(memberTags.begin(), memberTags.end());
  for (int i = 0; i < outbounds.size(); ++i) {
    if (i == groupIndex || !outbounds[i].isObject()) {
      continue;
    }
    const QJsonObject ob   = outbounds[i].toObject();
    const QString     type = ob.value("type").toString();
    if (type != "selector" && type != "urltest" && type != "fallback") {
      continue;
    }
    const QJsonArray members = ob.value("outbounds").toArray();
    for (const auto& m : members) {
      exclusiveTags.remove(m.toString().trimmed());
    }
  }

  // Collect tags to remove: the group itself + exclusive member nodes.
  QSet<QString> tagsToRemove;
  tagsToRemove.insert(groupTag);
  tagsToRemove.unite(exclusiveTags);

  // Remove from outbounds array.
  QJsonArray newOutbounds;
  for (int i = 0; i < outbounds.size(); ++i) {
    if (!outbounds[i].isObject()) {
      newOutbounds.append(outbounds[i]);
      continue;
    }
    const QString tag = outbounds[i].toObject().value("tag").toString();
    if (tagsToRemove.contains(tag)) {
      continue;
    }
    newOutbounds.append(outbounds[i]);
  }

  // Remove groupTag from manual selector's outbounds list.
  for (int i = 0; i < newOutbounds.size(); ++i) {
    if (!newOutbounds[i].isObject()) {
      continue;
    }
    QJsonObject ob = newOutbounds[i].toObject();
    if (ob.value("tag").toString() != ConfigConstants::TAG_MANUAL ||
        ob.value("type").toString() != "selector") {
      continue;
    }
    QJsonArray  current = ob.value("outbounds").toArray();
    QJsonArray  filtered;
    for (const auto& item : current) {
      if (item.toString().trimmed() != groupTag) {
        filtered.append(item);
      }
    }
    ob["outbounds"]  = filtered;
    newOutbounds[i]  = ob;
    break;
  }

  // Update route rules: change any rule whose outbound references the deleted
  // group to fall back to "manual".
  if (config.contains("route") && config["route"].isObject()) {
    QJsonObject route = config.value("route").toObject();
    if (route.contains("rules") && route["rules"].isArray()) {
      QJsonArray rules = route.value("rules").toArray();
      bool       changed = false;
      for (int i = 0; i < rules.size(); ++i) {
        if (!rules[i].isObject()) {
          continue;
        }
        QJsonObject rule = rules[i].toObject();
        if (rule.value("outbound").toString() == groupTag) {
          rule["outbound"] = ConfigConstants::TAG_MANUAL;
          rules[i]         = rule;
          changed          = true;
        }
      }
      if (changed) {
        route["rules"]  = rules;
        config["route"] = route;
      }
    }
  }

  config["outbounds"] = newOutbounds;
  if (!m_configRepo->saveConfig(active.configPath, config)) {
    setError(tr("Failed to save config: %1").arg(active.configPath));
    return false;
  }
  emit applyConfigRequested(active.configPath, true);
  return true;
}

void SubscriptionService::syncRuleSetToSubscriptions(
    const QString& ruleSetName) {
  const QString name = ruleSetName.isEmpty() ? QStringLiteral("default") : ruleSetName;
  for (auto& sub : m_subscriptions) {
    if (!sub.enableSharedRules) {
      continue;
    }
    const QStringList sets =
        sub.ruleSets.isEmpty() ? QStringList{"default"} : sub.ruleSets;
    if (!sets.contains(name)) {
      continue;
    }
    syncSharedRulesToConfig(sub);
  }
  if (m_activeIndex >= 0 && m_activeIndex < m_subscriptions.count()) {
    const auto& active = m_subscriptions[m_activeIndex];
    if (active.enableSharedRules && !active.configPath.isEmpty()) {
      const QStringList sets =
          active.ruleSets.isEmpty() ? QStringList{"default"} : active.ruleSets;
      if (sets.contains(name)) {
        requestCoalescedApplyConfig(active.configPath);
      }
    }
  }
}

void SubscriptionService::syncAllRuleSetsToSubscriptions() {
  for (auto& sub : m_subscriptions) {
    if (!sub.enableSharedRules) {
      continue;
    }
    syncSharedRulesToConfig(sub);
  }
  if (m_activeIndex >= 0 && m_activeIndex < m_subscriptions.count()) {
    const auto& active = m_subscriptions[m_activeIndex];
    if (active.enableSharedRules && !active.configPath.isEmpty()) {
      requestCoalescedApplyConfig(active.configPath);
    }
  }
}

void SubscriptionService::requestCoalescedApplyConfig(const QString& configPath) {
  if (configPath.isEmpty()) {
    return;
  }
  m_pendingApplyConfigPath = configPath;
  if (m_applyRequestScheduled) {
    return;
  }
  m_applyRequestScheduled = true;
  QTimer::singleShot(0, this, [this]() {
    m_applyRequestScheduled = false;
    if (m_pendingApplyConfigPath.isEmpty()) {
      return;
    }
    const QString pendingPath = m_pendingApplyConfigPath;
    m_pendingApplyConfigPath.clear();
    emit applyConfigRequested(pendingPath, true);
  });
}

QString SubscriptionService::getCurrentConfig() const {
  const QString path =
      m_activeConfigPath.isEmpty()
          ? (m_configRepo ? m_configRepo->getActiveConfigPath() : QString())
          : m_activeConfigPath;
  if (path.isEmpty()) {
    return QString();
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return QString();
  }
  const QString content = QString::fromUtf8(file.readAll());
  file.close();
  return content;
}

bool SubscriptionService::saveCurrentConfig(const QString& content,
                                            bool           applyRuntime) {
  QString targetPath = m_activeConfigPath;
  if (targetPath.isEmpty() && m_configRepo) {
    targetPath = m_configRepo->getActiveConfigPath();
  }
  if (targetPath.isEmpty()) {
    return false;
  }
  QJsonParseError err;
  QJsonDocument   doc = QJsonDocument::fromJson(content.toUtf8(), &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    return false;
  }
  if (!m_configRepo || !m_configRepo->saveConfig(targetPath, doc.object())) {
    return false;
  }
  if (applyRuntime) {
    emit applyConfigRequested(targetPath, true);
  }
  return true;
}

bool SubscriptionService::rollbackSubscriptionConfig(
    const QString& configPath) {
  return SubscriptionConfigStore::rollbackSubscriptionConfig(configPath);
}

bool SubscriptionService::deleteSubscriptionConfig(const QString& configPath) {
  return SubscriptionConfigStore::deleteSubscriptionConfig(configPath);
}

QList<SubscriptionInfo> SubscriptionService::getSubscriptions() const {
  return m_subscriptions;
}

int SubscriptionService::getActiveIndex() const {
  return m_activeIndex;
}

QString SubscriptionService::getActiveConfigPath() const {
  return m_activeConfigPath;
}

QString SubscriptionService::generateId() const {
  return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
