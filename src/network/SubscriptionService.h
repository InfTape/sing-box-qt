#ifndef SUBSCRIPTIONSERVICE_H
#define SUBSCRIPTIONSERVICE_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

class HttpClient;

struct SubscriptionInfo {
    QString id;
    QString name;
    QString url;
    qint64 lastUpdate;
    int nodeCount;
    bool enabled;
    bool isManual;
    QString manualContent;
    bool useOriginalConfig;
    QString configPath;
    QString backupPath;
    int autoUpdateIntervalMinutes;
    qint64 subscriptionUpload;
    qint64 subscriptionDownload;
    qint64 subscriptionTotal;
    qint64 subscriptionExpire;
};

class SubscriptionService : public QObject
{
    Q_OBJECT

public:
    explicit SubscriptionService(QObject *parent = nullptr);
    ~SubscriptionService();

    // 订阅管理
    void addUrlSubscription(const QString &url,
                            const QString &name,
                            bool useOriginalConfig,
                            int autoUpdateIntervalMinutes,
                            bool applyRuntime);
    void addManualSubscription(const QString &content,
                               const QString &name,
                               bool useOriginalConfig,
                               bool isUriList,
                               bool applyRuntime);
    void removeSubscription(const QString &id);
    void refreshSubscription(const QString &id, bool applyRuntime);
    void updateAllSubscriptions(bool applyRuntime);
    void updateSubscriptionMeta(const QString &id,
                                const QString &name,
                                const QString &url,
                                bool isManual,
                                const QString &manualContent,
                                bool useOriginalConfig,
                                int autoUpdateIntervalMinutes);
    void setActiveSubscription(const QString &id, bool applyRuntime);
    void clearActiveSubscription();

    QString getCurrentConfig() const;
    bool saveCurrentConfig(const QString &content, bool applyRuntime);
    bool rollbackSubscriptionConfig(const QString &configPath);
    bool deleteSubscriptionConfig(const QString &configPath);

    // 获取订阅列表
    QList<SubscriptionInfo> getSubscriptions() const;
    int getActiveIndex() const;
    QString getActiveConfigPath() const;
    
    // 解析订阅内容
    QJsonArray parseSubscriptionContent(const QByteArray &content) const;

signals:
    void subscriptionAdded(const SubscriptionInfo &info);
    void subscriptionRemoved(const QString &id);
    void subscriptionUpdated(const QString &id);
    void errorOccurred(const QString &error);
    void activeSubscriptionChanged(const QString &id, const QString &configPath);
    void applyConfigRequested(const QString &configPath, bool restart);
    void progressChanged(const QString &id, int progress);

private:
    void saveToDatabase();
    SubscriptionInfo* findSubscription(const QString &id);
    QString generateConfigFileName(const QString &name) const;
    bool isJsonContent(const QString &content) const;
    QJsonArray extractNodesWithFallback(const QString &content) const;
    bool saveConfigWithNodes(const QJsonArray &nodes,
                             const QString &targetPath);
    bool saveOriginalConfig(const QString &content,
                            const QString &targetPath);
    void updateSubscriptionUserinfo(SubscriptionInfo &info, const QJsonObject &headers);
    void updateSubscriptionUserinfoFromHeader(SubscriptionInfo &info, const QByteArray &header);

    // 解析不同格式
    QJsonArray parseSingBoxConfig(const QByteArray &content) const;
    QJsonArray parseClashConfig(const QByteArray &content) const;
    QJsonArray parseURIList(const QByteArray &content) const;
    
    // URI 解析
    QJsonObject parseVmessURI(const QString &uri) const;
    QJsonObject parseVlessURI(const QString &uri) const;
    QJsonObject parseTrojanURI(const QString &uri) const;
    QJsonObject parseShadowsocksURI(const QString &uri) const;
    QJsonObject parseHysteria2URI(const QString &uri) const;
    
    QString generateId() const;
    
    HttpClient *m_httpClient;
    QList<SubscriptionInfo> m_subscriptions;
    int m_activeIndex;
    QString m_activeConfigPath;
};

#endif // SUBSCRIPTIONSERVICE_H
