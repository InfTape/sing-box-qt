
#include "SubscriptionService.h"
#include "HttpClient.h"
#include "utils/Logger.h"
#include "storage/DatabaseService.h"
#include "storage/ConfigManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRegularExpression>
#include <yaml-cpp/yaml.h>

namespace {
constexpr qint64 kUnsetValue = -1;

QString sanitizeFileName(const QString &name)
{
    QString safe = name.toLower();
    safe.replace(QRegularExpression("[^a-z0-9-_]+"), "-");
    safe.replace(QRegularExpression("-+"), "-");
    safe.remove(QRegularExpression("^-|-$"));
    if (safe.isEmpty()) {
        safe = "subscription";
    }
    return safe;
}

QString tryDecodeBase64ToText(const QString &raw)
{
    QString compact = raw;
    compact.remove(QRegularExpression("\\s+"));
    if (compact.isEmpty()) {
        return QString();
    }

    int rem = compact.length() % 4;
    if (rem != 0) {
        compact.append(QString("=").repeated(4 - rem));
    }

    QByteArray input = compact.toUtf8();
    QByteArray decoded = QByteArray::fromBase64(input, QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded.isEmpty()) {
        return QString::fromUtf8(decoded);
    }

    decoded = QByteArray::fromBase64(input, QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded.isEmpty()) {
        return QString::fromUtf8(decoded);
    }

    return QString();
}

QJsonObject parseUserinfoHeader(const QByteArray &header)
{
    QJsonObject info;
    if (header.isEmpty()) {
        return info;
    }
    const QString raw = QString::fromUtf8(header).trimmed();
    const QStringList segments = raw.split(';', Qt::SkipEmptyParts);
    for (const QString &segment : segments) {
        const QStringList pair = segment.trimmed().split('=', Qt::SkipEmptyParts);
        if (pair.size() != 2) continue;
        const QString key = pair[0].trimmed().toLower();
        const qint64 value = pair[1].trimmed().toLongLong();
        if (value < 0) continue;
        if (key == "upload") info["upload"] = value;
        if (key == "download") info["download"] = value;
        if (key == "total") info["total"] = value;
        if (key == "expire") info["expire"] = value;
    }
    return info;
}
} // namespace

SubscriptionService::SubscriptionService(QObject *parent)
    : QObject(parent)
    , m_httpClient(new HttpClient(this))
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

QString SubscriptionService::generateConfigFileName(const QString &name) const
{
    const QString safe = sanitizeFileName(name);
    return QString("%1-%2.json").arg(safe).arg(QDateTime::currentMSecsSinceEpoch());
}

bool SubscriptionService::isJsonContent(const QString &content) const
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8(), &err);
    return err.error == QJsonParseError::NoError && doc.isObject();
}

QJsonArray SubscriptionService::extractNodesWithFallback(const QString &content) const
{
    QJsonArray nodes = parseSubscriptionContent(content.toUtf8());
    if (!nodes.isEmpty()) {
        return nodes;
    }

    const QString decoded = tryDecodeBase64ToText(content);
    if (!decoded.isEmpty()) {
        nodes = parseSubscriptionContent(decoded.toUtf8());
        if (!nodes.isEmpty()) {
            return nodes;
        }
    }

    QString stripped = content.trimmed();
    stripped.replace("vmess://", "");
    stripped.replace("vless://", "");
    stripped.replace("trojan://", "");
    stripped.replace("ss://", "");
    stripped.replace("hysteria2://", "");
    stripped.replace("hy2://", "");
    const QString decodedStripped = tryDecodeBase64ToText(stripped);
    if (!decodedStripped.isEmpty()) {
        nodes = parseSubscriptionContent(decodedStripped.toUtf8());
    }
    return nodes;
}

bool SubscriptionService::saveConfigWithNodes(const QJsonArray &nodes,
                                              const QString &targetPath)
{
    return ConfigManager::instance().generateConfigWithNodes(nodes, targetPath);
}

bool SubscriptionService::saveOriginalConfig(const QString &content,
                                             const QString &targetPath)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject config = doc.object();
    ConfigManager::instance().applyPortSettings(config);

    return ConfigManager::instance().saveConfig(targetPath, config);
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
    updateSubscriptionUserinfo(info, parseUserinfoHeader(header));
}
void SubscriptionService::addUrlSubscription(const QString &url,
                                             const QString &name,
                                             bool useOriginalConfig,
                                             int autoUpdateIntervalMinutes,
                                             bool applyRuntime)
{
    const QString trimmedUrl = url.trimmed();
    if (trimmedUrl.isEmpty()) {
        emit errorOccurred(tr("请输入订阅链接"));
        return;
    }

    QString subName = name.trimmed().isEmpty() ? QUrl(trimmedUrl).host() : name.trimmed();
    QString id = generateId();
    const QString configName = generateConfigFileName(subName);
    const QString configPath = ConfigManager::instance().getConfigDir() + "/" + configName;

    Logger::info(QString("添加订阅: %1 (%2)").arg(subName, trimmedUrl));

    QNetworkRequest request{QUrl(trimmedUrl)};
    request.setTransferTimeout(30000);
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkReply *reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        manager->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(tr("获取订阅失败"));
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
                emit errorOccurred(tr("原始订阅仅支持 sing-box JSON 配置内容"));
                return;
            }
            saved = saveOriginalConfig(QString::fromUtf8(data), configPath);
        } else {
            QJsonArray nodes = extractNodesWithFallback(QString::fromUtf8(data));
            if (nodes.isEmpty()) {
                emit errorOccurred(tr("无法从订阅内容提取节点，请检查订阅格式"));
                return;
            }
            info.nodeCount = nodes.count();
            saved = saveConfigWithNodes(nodes, configPath);
            DatabaseService::instance().saveSubscriptionNodes(id, nodes);
        }

        if (!saved) {
            emit errorOccurred(tr("保存订阅配置失败"));
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
        emit errorOccurred(tr("请输入订阅内容"));
        return;
    }

    if (useOriginalConfig && !isJsonContent(trimmed)) {
        emit errorOccurred(tr("原始订阅仅支持 sing-box JSON 配置内容"));
        return;
    }

    QString subName = name.trimmed().isEmpty() ? tr("手动订阅") : name.trimmed();
    QString id = generateId();
    const QString configName = generateConfigFileName(subName);
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
        saved = saveOriginalConfig(trimmed, configPath);
    } else {
        QJsonArray nodes = extractNodesWithFallback(trimmed);
        if (nodes.isEmpty()) {
            emit errorOccurred(tr("无法从订阅内容提取节点，请检查格式"));
            return;
        }
        info.nodeCount = nodes.count();
        saved = saveConfigWithNodes(nodes, configPath);
        DatabaseService::instance().saveSubscriptionNodes(id, nodes);
    }

    if (!saved) {
        emit errorOccurred(tr("保存订阅配置失败"));
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
        emit errorOccurred(tr("订阅不存在"));
        return;
    }

    if (sub->isManual) {
        if (sub->manualContent.trimmed().isEmpty()) {
            emit errorOccurred(tr("手动订阅内容为空"));
            return;
        }
        if (sub->useOriginalConfig && !isJsonContent(sub->manualContent)) {
            emit errorOccurred(tr("原始订阅仅支持 sing-box JSON 配置内容"));
            return;
        }

        bool saved = false;
        if (sub->useOriginalConfig) {
            saved = saveOriginalConfig(sub->manualContent, sub->configPath);
        } else {
            QJsonArray nodes = extractNodesWithFallback(sub->manualContent);
            if (nodes.isEmpty()) {
                emit errorOccurred(tr("无法从订阅内容提取节点，请检查格式"));
                return;
            }
            sub->nodeCount = nodes.count();
            saved = saveConfigWithNodes(nodes, sub->configPath);
            DatabaseService::instance().saveSubscriptionNodes(sub->id, nodes);
        }
        if (!saved) {
            emit errorOccurred(tr("刷新订阅失败"));
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
        emit errorOccurred(tr("订阅链接为空"));
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
            emit errorOccurred(tr("更新订阅失败"));
            return;
        }

        const QByteArray data = reply->readAll();
        const QByteArray userinfoHeader = reply->rawHeader("subscription-userinfo");
        bool saved = false;
        if (sub->useOriginalConfig) {
            if (!isJsonContent(QString::fromUtf8(data))) {
                emit errorOccurred(tr("原始订阅仅支持 sing-box JSON 配置内容"));
                return;
            }
            saved = saveOriginalConfig(QString::fromUtf8(data), sub->configPath);
        } else {
            QJsonArray nodes = extractNodesWithFallback(QString::fromUtf8(data));
            if (nodes.isEmpty()) {
                emit errorOccurred(tr("无法从订阅内容提取节点，请检查订阅格式"));
                return;
            }
            sub->nodeCount = nodes.count();
            saved = saveConfigWithNodes(nodes, sub->configPath);
            DatabaseService::instance().saveSubscriptionNodes(sub->id, nodes);
        }

        if (!saved) {
            emit errorOccurred(tr("保存订阅配置失败"));
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
        emit errorOccurred(tr("订阅不存在"));
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
    emit errorOccurred(tr("订阅不存在"));
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
    QFileInfo pathInfo(configPath);
    if (!pathInfo.exists()) {
        return false;
    }
    const QString backupPath = configPath + ".bak";
    if (!QFile::exists(backupPath)) {
        return false;
    }
    QFile::remove(configPath);
    return QFile::copy(backupPath, configPath);
}

bool SubscriptionService::deleteSubscriptionConfig(const QString &configPath)
{
    if (configPath.isEmpty()) {
        return false;
    }
    if (QFile::exists(configPath)) {
        QFile::remove(configPath);
    }
    const QString backup = configPath + ".bak";
    if (QFile::exists(backup)) {
        QFile::remove(backup);
    }
    return true;
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

QJsonArray SubscriptionService::parseSubscriptionContent(const QByteArray &content) const
{
    QJsonDocument doc = QJsonDocument::fromJson(content);
    if (!doc.isNull() && doc.isObject()) {
        return parseSingBoxConfig(content);
    }

    QString str = QString::fromUtf8(content);
    if (str.contains("proxies:")) {
        return parseClashConfig(content);
    }

    return parseURIList(content);
}

QJsonArray SubscriptionService::parseSingBoxConfig(const QByteArray &content) const
{
    QJsonDocument doc = QJsonDocument::fromJson(content);
    if (doc.isObject() && doc.object().contains("outbounds")) {
        return doc.object()["outbounds"].toArray();
    }
    return QJsonArray();
}

QJsonArray SubscriptionService::parseClashConfig(const QByteArray &content) const
{
    QJsonArray nodes;

    try {
        YAML::Node yaml = YAML::Load(content.toStdString());
        if (yaml["proxies"]) {
            for (const auto &proxy : yaml["proxies"]) {
                QJsonObject node;
                node["type"] = QString::fromStdString(proxy["type"].as<std::string>());
                node["tag"] = QString::fromStdString(proxy["name"].as<std::string>());
                node["server"] = QString::fromStdString(proxy["server"].as<std::string>());
                node["server_port"] = proxy["port"].as<int>();

                std::string type = proxy["type"].as<std::string>();
                if (type == "vmess") {
                    node["uuid"] = QString::fromStdString(proxy["uuid"].as<std::string>());
                    if (proxy["alterId"]) {
                        node["alter_id"] = proxy["alterId"].as<int>();
                    }
                } else if (type == "trojan") {
                    node["password"] = QString::fromStdString(proxy["password"].as<std::string>());
                } else if (type == "ss") {
                    node["type"] = "shadowsocks";
                    node["method"] = QString::fromStdString(proxy["cipher"].as<std::string>());
                    node["password"] = QString::fromStdString(proxy["password"].as<std::string>());
                }

                nodes.append(node);
            }
        }
    } catch (const std::exception &e) {
        Logger::error(QString("YAML 解析错误: %1").arg(e.what()));
    }

    return nodes;
}

QJsonArray SubscriptionService::parseURIList(const QByteArray &content) const
{
    QJsonArray nodes;

    QByteArray decoded = QByteArray::fromBase64(content);
    QString text = decoded.isEmpty()
        ? QString::fromUtf8(content)
        : QString::fromUtf8(decoded);

    QStringList lines = text.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        QString uri = line.trimmed();
        QJsonObject node;

        if (uri.startsWith("vmess://")) {
            node = parseVmessURI(uri);
        } else if (uri.startsWith("vless://")) {
            node = parseVlessURI(uri);
        } else if (uri.startsWith("trojan://")) {
            node = parseTrojanURI(uri);
        } else if (uri.startsWith("ss://")) {
            node = parseShadowsocksURI(uri);
        } else if (uri.startsWith("hysteria2://") || uri.startsWith("hy2://")) {
            node = parseHysteria2URI(uri);
        }

        if (!node.isEmpty()) {
            nodes.append(node);
        }
    }

    return nodes;
}

QJsonObject SubscriptionService::parseVmessURI(const QString &uri) const
{
    QJsonObject node;
    QString encoded = uri.mid(8);
    QByteArray decoded = QByteArray::fromBase64(encoded.toUtf8());

    QJsonDocument doc = QJsonDocument::fromJson(decoded);
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        node["type"] = "vmess";
        node["tag"] = obj["ps"].toString();
        node["server"] = obj["add"].toString();
        node["server_port"] = obj["port"].toVariant().toInt();
        node["uuid"] = obj["id"].toString();
        node["alter_id"] = obj["aid"].toVariant().toInt();
    }

    return node;
}

QJsonObject SubscriptionService::parseVlessURI(const QString &uri) const
{
    QJsonObject node;
    QUrl url(uri);
    QUrlQuery query(url.query());

    node["type"] = "vless";
    node["tag"] = QUrl::fromPercentEncoding(url.fragment().toUtf8());
    node["server"] = url.host();
    node["server_port"] = url.port();
    node["uuid"] = url.userName();

    QString flow = query.queryItemValue("flow");
    if (!flow.isEmpty()) {
        node["flow"] = flow;
    }

    QString security = query.queryItemValue("security");
    if (security == "tls" || security == "reality") {
        QJsonObject tls;
        tls["enabled"] = true;

        QString sni = query.queryItemValue("sni");
        if (!sni.isEmpty()) {
            tls["server_name"] = sni;
        }

        if (query.queryItemValue("allowInsecure") == "1") {
            tls["insecure"] = true;
        }

        QString alpn = query.queryItemValue("alpn");
        if (!alpn.isEmpty()) {
            tls["alpn"] = QJsonArray::fromStringList(alpn.split(","));
        }

        if (query.hasQueryItem("fp")) {
            QJsonObject utls;
            utls["enabled"] = true;
            utls["fingerprint"] = query.queryItemValue("fp");
            tls["utls"] = utls;
        }

        if (security == "reality") {
            QJsonObject reality;
            reality["enabled"] = true;
            reality["public_key"] = query.queryItemValue("pbk");
            QString shortId = query.queryItemValue("sid");
            if (!shortId.isEmpty()) {
                reality["short_id"] = shortId;
            }
            tls["reality"] = reality;
        }

        node["tls"] = tls;
    }

    node["packet_encoding"] = "xudp";

    QString type = query.queryItemValue("type");
    if (type == "ws") {
        QJsonObject transport;
        transport["type"] = "ws";

        QString path = query.queryItemValue("path");
        if (!path.isEmpty()) {
            transport["path"] = path;
        }

        QString host = query.queryItemValue("host");
        if (!host.isEmpty()) {
            QJsonObject headers;
            headers["Host"] = host;
            transport["headers"] = headers;
        }

        node["transport"] = transport;
    } else if (type == "grpc") {
        QJsonObject transport;
        transport["type"] = "grpc";

        QString serviceName = query.queryItemValue("serviceName");
        if (!serviceName.isEmpty()) {
            transport["service_name"] = serviceName;
        }

        node["transport"] = transport;
    } else if (type == "h2" || type == "http") {
        QJsonObject transport;
        transport["type"] = "http";

        QString path = query.queryItemValue("path");
        if (!path.isEmpty()) {
            transport["path"] = path;
        }

        QString host = query.queryItemValue("host");
        if (!host.isEmpty()) {
            transport["host"] = QJsonArray::fromStringList(host.split(","));
        }

        node["transport"] = transport;
    }

    return node;
}

QJsonObject SubscriptionService::parseTrojanURI(const QString &uri) const
{
    QJsonObject node;
    QUrl url(uri);
    QUrlQuery query(url.query());

    node["type"] = "trojan";
    node["tag"] = QUrl::fromPercentEncoding(url.fragment().toUtf8());
    node["server"] = url.host();
    node["server_port"] = url.port();
    node["password"] = url.userName();

    QJsonObject tls;
    tls["enabled"] = true;
    tls["server_name"] = query.queryItemValue("sni");
    tls["insecure"] = query.queryItemValue("allowInsecure") == "1";
    node["tls"] = tls;

    return node;
}

QJsonObject SubscriptionService::parseShadowsocksURI(const QString &uri) const
{
    QJsonObject node;
    QString data = uri.mid(5);

    int hashIndex = data.indexOf('#');
    QString tag;
    if (hashIndex != -1) {
        tag = QUrl::fromPercentEncoding(data.mid(hashIndex + 1).toUtf8());
        data = data.left(hashIndex);
    }

    QString userInfo;
    QString hostPart;

    if (data.contains("@")) {
        QStringList parts = data.split("@");
        QByteArray decoded = QByteArray::fromBase64(parts.first().toUtf8());
        if (decoded.contains(':')) {
            userInfo = QString::fromUtf8(decoded);
        } else {
            userInfo = parts.first();
        }
        hostPart = parts.last();
    } else {
        QByteArray decoded = QByteArray::fromBase64(data.toUtf8());
        QString full = QString::fromUtf8(decoded);
        int atIndex = full.lastIndexOf('@');
        if (atIndex != -1) {
            userInfo = full.left(atIndex);
            hostPart = full.mid(atIndex + 1);
        }
    }

    if (!userInfo.isEmpty()) {
        int colonIndex = userInfo.indexOf(':');
        if (colonIndex != -1) {
            node["type"] = "shadowsocks";
            node["tag"] = tag.isEmpty() ? hostPart : tag;
            node["method"] = userInfo.left(colonIndex);
            node["password"] = userInfo.mid(colonIndex + 1);

            int portIndex = hostPart.lastIndexOf(':');
            if (portIndex != -1) {
                node["server"] = hostPart.left(portIndex);
                node["server_port"] = hostPart.mid(portIndex + 1).toInt();
            }
        }
    }

    return node;
}

QJsonObject SubscriptionService::parseHysteria2URI(const QString &uri) const
{
    QJsonObject node;
    QString uriFixed = uri;
    if (uriFixed.startsWith("hy2://")) {
        uriFixed = "hysteria2://" + uriFixed.mid(6);
    }

    QUrl url(uriFixed);
    QUrlQuery query(url.query());

    node["type"] = "hysteria2";
    node["tag"] = QUrl::fromPercentEncoding(url.fragment().toUtf8());
    node["server"] = url.host();
    node["server_port"] = url.port();
    node["password"] = url.userName();

    if (node["password"].toString().isEmpty()) {
        node["password"] = query.queryItemValue("auth");
    }

    QJsonObject tls;
    tls["enabled"] = true;
    tls["server_name"] = query.queryItemValue("sni");
    tls["insecure"] = query.queryItemValue("insecure") == "1";
    node["tls"] = tls;

    if (query.hasQueryItem("obfs")) {
        QJsonObject obfs;
        obfs["type"] = query.queryItemValue("obfs");
        obfs["password"] = query.queryItemValue("obfs-password");
        node["obfs"] = obfs;
    }

    return node;
}

QString SubscriptionService::generateId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
