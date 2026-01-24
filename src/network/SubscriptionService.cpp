
#include "SubscriptionService.h"
#include "services/subscription/SubscriptionParser.h"
#include "storage/DatabaseService.h"
#include "storage/SubscriptionConfigStore.h"
#include "services/ConfigManager.h"
#include "utils/Logger.h"
#include "utils/SubscriptionUserinfo.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrl>
#include <QUuid>
#include <QDateTime>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

namespace {
constexpr qint64 kUnsetValue = -1;
} // namespace

SubscriptionService::SubscriptionService(QObject *parent)
    : QObject(parent)
    , m_activeIndex(-1)
{
    QJsonArray subs = DatabaseService::instance().getSubscriptions();
    for (const auto &val : subs) {
        QJsonObject obj = val.toObject();
        SubscriptionInfo info;
        info.id = obj.value("id").toString();
        info.name = obj.value("name").toString();
        info.url = obj.value("url").toString();
        info.lastUpdate = obj.value("last_update").toVariant().toLongLong();
        if (info.lastUpdate <= 0 && obj.value("last_update").isString()) {
            info.lastUpdate = QDateTime::fromString(obj.value("last_update").toString(), Qt::ISODate).toMSecsSinceEpoch();
        }
        info.nodeCount = obj.value("node_count").toInt();
        info.enabled = obj.value("enabled").toBool(true);
        info.isManual = obj.value("is_manual").toBool(false);
        info.manualContent = obj.value("manual_content").toString();
        info.useOriginalConfig = obj.value("use_original_config").toBool(false);
        info.configPath = obj.value("config_path").toString();
        info.backupPath = obj.value("backup_path").toString();
        info.autoUpdateIntervalMinutes = obj.value("auto_update_interval_minutes").toInt(720);
        info.subscriptionUpload = obj.value("subscription_upload").toVariant().toLongLong();
        info.subscriptionDownload = obj.value("subscription_download").toVariant().toLongLong();
        info.subscriptionTotal = obj.value("subscription_total").toVariant().toLongLong();
        info.subscriptionExpire = obj.value("subscription_expire").toVariant().toLongLong();
        if (!obj.contains("subscription_upload")) info.subscriptionUpload = kUnsetValue;
        if (!obj.contains("subscription_download")) info.subscriptionDownload = kUnsetValue;
        if (!obj.contains("subscription_total")) info.subscriptionTotal = kUnsetValue;
        if (!obj.contains("subscription_expire")) info.subscriptionExpire = kUnsetValue;
        m_subscriptions.append(info);
    }

    m_activeIndex = DatabaseService::instance().getActiveSubscriptionIndex();
    m_activeConfigPath = DatabaseService::instance().getActiveConfigPath();
}

SubscriptionService::~SubscriptionService() = default;

void SubscriptionService::saveToDatabase()
{
    QJsonArray array;
    for (const auto &info : m_subscriptions) {
        QJsonObject obj;
        obj["id"] = info.id;
        obj["name"] = info.name;
        obj["url"] = info.url;
        obj["last_update"] = info.lastUpdate;
        obj["node_count"] = info.nodeCount;
        obj["enabled"] = info.enabled;
        obj["is_manual"] = info.isManual;
        obj["manual_content"] = info.manualContent;
        obj["use_original_config"] = info.useOriginalConfig;
        obj["config_path"] = info.configPath;
        obj["backup_path"] = info.backupPath;
        obj["auto_update_interval_minutes"] = info.autoUpdateIntervalMinutes;
        if (info.subscriptionUpload >= 0) obj["subscription_upload"] = info.subscriptionUpload;
        if (info.subscriptionDownload >= 0) obj["subscription_download"] = info.subscriptionDownload;
        if (info.subscriptionTotal >= 0) obj["subscription_total"] = info.subscriptionTotal;
        if (info.subscriptionExpire >= 0) obj["subscription_expire"] = info.subscriptionExpire;
        array.append(obj);
    }
    DatabaseService::instance().saveSubscriptions(array);
    DatabaseService::instance().saveActiveSubscriptionIndex(m_activeIndex);
    DatabaseService::instance().saveActiveConfigPath(m_activeConfigPath);
}

SubscriptionInfo* SubscriptionService::findSubscription(const QString &id)
{
    for (auto &sub : m_subscriptions) {
        if (sub.id == id) {
            return &sub;
        }
    }
    return nullptr;
}

bool SubscriptionService::isJsonContent(const QString &content) const
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8(), &err);
    return err.error == QJsonParseError::NoError && doc.isObject();
}

void SubscriptionService::updateSubscriptionUserinfo(SubscriptionInfo &info, const QJsonObject &headers)
{
    if (headers.isEmpty()) {
        info.subscriptionUpload = kUnsetValue;
        info.subscriptionDownload = kUnsetValue;
        info.subscriptionTotal = kUnsetValue;
        info.subscriptionExpire = kUnsetValue;
        return;
    }

    info.subscriptionUpload = headers.value("upload").toVariant().toLongLong();
    info.subscriptionDownload = headers.value("download").toVariant().toLongLong();
    info.subscriptionTotal = headers.value("total").toVariant().toLongLong();
    info.subscriptionExpire = headers.value("expire").toVariant().toLongLong();
}

void SubscriptionService::updateSubscriptionUserinfoFromHeader(SubscriptionInfo &info, const QByteArray &header)
{
    updateSubscriptionUserinfo(info, SubscriptionUserinfo::parseUserinfoHeader(header));
}
void SubscriptionService::addUrlSubscription(const QString &url,
                                             const QString &name,
                                             bool useOriginalConfig,
                                             int autoUpdateIntervalMinutes,
                                             bool applyRuntime)
{
    const QString trimmedUrl = url.trimmed();
    if (trimmedUrl.isEmpty()) {
        emit errorOccurred(tr("Please enter a subscription URL"));
        return;
    }

    QString subName = name.trimmed().isEmpty() ? QUrl(trimmedUrl).host() : name.trimmed();
    QString id = generateId();
    const QString configName = SubscriptionConfigStore::generateConfigFileName(subName);
    const QString configPath = ConfigManager::instance().getConfigDir() + "/" + configName;

    Logger::info(QString("Add subscription: %1 (%2)").arg(subName, trimmedUrl));

    QNetworkRequest request{QUrl(trimmedUrl)};
    request.setTransferTimeout(30000);
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkReply *reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        manager->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(tr("Failed to fetch subscription"));
            return;
        }

        const QByteArray data = reply->readAll();
        const QByteArray userinfoHeader = reply->rawHeader("subscription-userinfo");

        SubscriptionInfo info;
        info.id = id;
        info.name = subName;
        info.url = trimmedUrl;
        info.enabled = true;
        info.isManual = false;
        info.manualContent.clear();
        info.useOriginalConfig = useOriginalConfig;
        info.autoUpdateIntervalMinutes = autoUpdateIntervalMinutes;
        info.configPath = configPath;
        info.backupPath = configPath + ".bak";

        bool saved = false;
        if (useOriginalConfig) {
            if (!isJsonContent(QString::fromUtf8(data))) {
                emit errorOccurred(tr("Original subscription only supports sing-box JSON config"));
                return;
            }
            saved = SubscriptionConfigStore::saveOriginalConfig(QString::fromUtf8(data), configPath);
        } else {
            QJsonArray nodes = SubscriptionParser::extractNodesWithFallback(QString::fromUtf8(data));
            if (nodes.isEmpty()) {
                emit errorOccurred(tr("Failed to extract nodes from subscription content; check format"));
                return;
            }
            info.nodeCount = nodes.count();
            saved = SubscriptionConfigStore::saveConfigWithNodes(nodes, configPath);
            DatabaseService::instance().saveSubscriptionNodes(id, nodes);
        }

        if (!saved) {
            emit errorOccurred(tr("Failed to save subscription config"));
            return;
        }

        info.lastUpdate = QDateTime::currentMSecsSinceEpoch();
        updateSubscriptionUserinfoFromHeader(info, userinfoHeader);

        m_subscriptions.append(info);
        m_activeIndex = m_subscriptions.count() - 1;
        m_activeConfigPath = configPath;
        saveToDatabase();

        emit subscriptionAdded(info);
        emit activeSubscriptionChanged(info.id, info.configPath);
        if (applyRuntime) {
            emit applyConfigRequested(info.configPath, true);
        }
    });
}

void SubscriptionService::addManualSubscription(const QString &content,
                                                const QString &name,
                                                bool useOriginalConfig,
                                                bool isUriList,
                                                bool applyRuntime)
{
    Q_UNUSED(isUriList)

    const QString trimmed = content.trimmed();
    if (trimmed.isEmpty()) {
        emit errorOccurred(tr("Please enter subscription content"));
        return;
    }

    if (useOriginalConfig && !isJsonContent(trimmed)) {
        emit errorOccurred(tr("Original subscription only supports sing-box JSON config"));
        return;
    }

    QString subName = name.trimmed().isEmpty() ? tr("Manual subscription") : name.trimmed();
    QString id = generateId();
    const QString configName = SubscriptionConfigStore::generateConfigFileName(subName);
    const QString configPath = ConfigManager::instance().getConfigDir() + "/" + configName;

    SubscriptionInfo info;
    info.id = id;
    info.name = subName;
    info.url.clear();
    info.enabled = true;
    info.isManual = true;
    info.manualContent = trimmed;
    info.useOriginalConfig = useOriginalConfig;
    info.autoUpdateIntervalMinutes = 0;
    info.configPath = configPath;
    info.backupPath = configPath + ".bak";

    bool saved = false;
    if (useOriginalConfig) {
        saved = SubscriptionConfigStore::saveOriginalConfig(trimmed, configPath);
    } else {
        QJsonArray nodes = SubscriptionParser::extractNodesWithFallback(trimmed);
        if (nodes.isEmpty()) {
            emit errorOccurred(tr("Failed to extract nodes from subscription content; check format"));
            return;
        }
        info.nodeCount = nodes.count();
        saved = SubscriptionConfigStore::saveConfigWithNodes(nodes, configPath);
        DatabaseService::instance().saveSubscriptionNodes(id, nodes);
    }

    if (!saved) {
        emit errorOccurred(tr("Failed to save subscription config"));
        return;
    }

    info.lastUpdate = QDateTime::currentMSecsSinceEpoch();
    info.subscriptionUpload = kUnsetValue;
    info.subscriptionDownload = kUnsetValue;
    info.subscriptionTotal = kUnsetValue;
    info.subscriptionExpire = kUnsetValue;

    m_subscriptions.append(info);
    m_activeIndex = m_subscriptions.count() - 1;
    m_activeConfigPath = configPath;
    saveToDatabase();

    emit subscriptionAdded(info);
    emit activeSubscriptionChanged(info.id, info.configPath);
    if (applyRuntime) {
        emit applyConfigRequested(info.configPath, true);
    }
}

void SubscriptionService::removeSubscription(const QString &id)
{
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

void SubscriptionService::refreshSubscription(const QString &id, bool applyRuntime)
{
    SubscriptionInfo *sub = findSubscription(id);
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
            emit errorOccurred(tr("Original subscription only supports sing-box JSON config"));
            return;
        }

        bool saved = false;
        if (sub->useOriginalConfig) {
            saved = SubscriptionConfigStore::saveOriginalConfig(sub->manualContent, sub->configPath);
        } else {
            QJsonArray nodes = SubscriptionParser::extractNodesWithFallback(sub->manualContent);
            if (nodes.isEmpty()) {
                emit errorOccurred(tr("Failed to extract nodes from subscription content; check format"));
                return;
            }
            sub->nodeCount = nodes.count();
            saved = SubscriptionConfigStore::saveConfigWithNodes(nodes, sub->configPath);
            DatabaseService::instance().saveSubscriptionNodes(sub->id, nodes);
        }
        if (!saved) {
            emit errorOccurred(tr("Failed to refresh subscription"));
            return;
        }
        sub->lastUpdate = QDateTime::currentMSecsSinceEpoch();
        sub->subscriptionUpload = kUnsetValue;
        sub->subscriptionDownload = kUnsetValue;
        sub->subscriptionTotal = kUnsetValue;
        sub->subscriptionExpire = kUnsetValue;
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
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkReply *reply = manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        manager->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(tr("Failed to update subscription"));
            return;
        }

        const QByteArray data = reply->readAll();
        const QByteArray userinfoHeader = reply->rawHeader("subscription-userinfo");
        bool saved = false;
        if (sub->useOriginalConfig) {
            if (!isJsonContent(QString::fromUtf8(data))) {
                emit errorOccurred(tr("Original subscription only supports sing-box JSON config"));
                return;
            }
            saved = SubscriptionConfigStore::saveOriginalConfig(QString::fromUtf8(data), sub->configPath);
        } else {
            QJsonArray nodes = SubscriptionParser::extractNodesWithFallback(QString::fromUtf8(data));
            if (nodes.isEmpty()) {
                emit errorOccurred(tr("Failed to extract nodes from subscription content; check format"));
                return;
            }
            sub->nodeCount = nodes.count();
            saved = SubscriptionConfigStore::saveConfigWithNodes(nodes, sub->configPath);
            DatabaseService::instance().saveSubscriptionNodes(sub->id, nodes);
        }

        if (!saved) {
            emit errorOccurred(tr("Failed to save subscription config"));
            return;
        }

        sub->lastUpdate = QDateTime::currentMSecsSinceEpoch();
        updateSubscriptionUserinfoFromHeader(*sub, userinfoHeader);
        saveToDatabase();
        emit subscriptionUpdated(sub->id);
        if (applyRuntime) {
            emit applyConfigRequested(sub->configPath, true);
        }
    });
}

void SubscriptionService::updateAllSubscriptions(bool applyRuntime)
{
    for (const auto &sub : m_subscriptions) {
        if (sub.enabled) {
            refreshSubscription(sub.id, applyRuntime);
        }
    }
}

void SubscriptionService::updateSubscriptionMeta(const QString &id,
                                                 const QString &name,
                                                 const QString &url,
                                                 bool isManual,
                                                 const QString &manualContent,
                                                 bool useOriginalConfig,
                                                 int autoUpdateIntervalMinutes)
{
    SubscriptionInfo *sub = findSubscription(id);
    if (!sub) {
        emit errorOccurred(tr("Subscription not found"));
        return;
    }

    sub->name = name.trimmed();
    sub->url = url.trimmed();
    sub->isManual = isManual;
    sub->manualContent = manualContent;
    sub->useOriginalConfig = useOriginalConfig;
    sub->autoUpdateIntervalMinutes = autoUpdateIntervalMinutes;
    saveToDatabase();
    emit subscriptionUpdated(id);
}

void SubscriptionService::setActiveSubscription(const QString &id, bool applyRuntime)
{
    for (int i = 0; i < m_subscriptions.count(); ++i) {
        if (m_subscriptions[i].id == id) {
            m_activeIndex = i;
            m_activeConfigPath = m_subscriptions[i].configPath;
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

void SubscriptionService::clearActiveSubscription()
{
    m_activeIndex = -1;
    m_activeConfigPath.clear();
    saveToDatabase();
    emit activeSubscriptionChanged(QString(), QString());
}
QString SubscriptionService::getCurrentConfig() const
{
    const QString path = m_activeConfigPath.isEmpty()
        ? ConfigManager::instance().getActiveConfigPath()
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

bool SubscriptionService::saveCurrentConfig(const QString &content, bool applyRuntime)
{
    QString targetPath = m_activeConfigPath;
    if (targetPath.isEmpty()) {
        targetPath = ConfigManager::instance().getActiveConfigPath();
    }
    if (targetPath.isEmpty()) {
        return false;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    if (!ConfigManager::instance().saveConfig(targetPath, doc.object())) {
        return false;
    }

    if (applyRuntime) {
        emit applyConfigRequested(targetPath, true);
    }
    return true;
}

bool SubscriptionService::rollbackSubscriptionConfig(const QString &configPath)
{
    return SubscriptionConfigStore::rollbackSubscriptionConfig(configPath);
}

bool SubscriptionService::deleteSubscriptionConfig(const QString &configPath)
{
    return SubscriptionConfigStore::deleteSubscriptionConfig(configPath);
}

QList<SubscriptionInfo> SubscriptionService::getSubscriptions() const
{
    return m_subscriptions;
}

int SubscriptionService::getActiveIndex() const
{
    return m_activeIndex;
}

QString SubscriptionService::getActiveConfigPath() const
{
    return m_activeConfigPath;
}

QString SubscriptionService::generateId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
