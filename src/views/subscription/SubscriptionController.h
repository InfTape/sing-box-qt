#ifndef SUBSCRIPTIONCONTROLLER_H
#define SUBSCRIPTIONCONTROLLER_H

#include <QString>
#include <QStringList>
#include <QJsonArray>
#include <QList>

class SubscriptionService;
struct SubscriptionInfo;

/**
 * @brief SubscriptionController 封装订阅业务操作，减少 View 直接触碰 Service。
 */
class SubscriptionController
{
public:
    explicit SubscriptionController(SubscriptionService *service);

    SubscriptionService* service() const { return m_service; }

    QList<SubscriptionInfo> subscriptions() const;
    int activeIndex() const;
    QString activeConfigPath() const;
    QString currentConfig() const;

    void addUrl(const QString &url,
                const QString &name,
                bool useOriginalConfig,
                int autoUpdateIntervalMinutes,
                bool applyRuntime,
                bool enableSharedRules,
                const QStringList &ruleSets);

    void addManual(const QString &content,
                   const QString &name,
                   bool useOriginalConfig,
                   bool isUriList,
                   bool applyRuntime,
                   bool enableSharedRules,
                   const QStringList &ruleSets);

    void refresh(const QString &id, bool applyRuntime);
    bool rollback(const QString &id);
    void remove(const QString &id);
    void removeSubscription(const QString &id) { remove(id); }

    void updateSubscription(const QString &id,
                            const QString &name,
                            const QString &url,
                            bool isManual,
                            const QString &content,
                            bool useOriginalConfig,
                            int autoUpdateIntervalMinutes,
                            bool enableSharedRules,
                            const QStringList &ruleSets);

    bool saveCurrentConfig(const QString &content, bool applyRuntime);

private:
    SubscriptionService *m_service;
};

#endif // SUBSCRIPTIONCONTROLLER_H
