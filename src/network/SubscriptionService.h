#ifndef SUBSCRIPTIONSERVICE_H
#define SUBSCRIPTIONSERVICE_H

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

class ConfigRepository;
struct SubscriptionInfo {
  QString     id;
  QString     name;
  QString     url;
  qint64      lastUpdate;
  int         nodeCount;
  bool        enabled;
  bool        isManual;
  QString     manualContent;
  bool        useOriginalConfig;
  QString     configPath;
  QString     backupPath;
  int         autoUpdateIntervalMinutes;
  qint64      subscriptionUpload;
  qint64      subscriptionDownload;
  qint64      subscriptionTotal;
  qint64      subscriptionExpire;
  bool        enableSharedRules = true;
  QStringList ruleSets;
};
class SubscriptionService : public QObject {
  Q_OBJECT

 public:
  explicit SubscriptionService(ConfigRepository* configRepo, QObject* parent = nullptr);
  ~SubscriptionService();

  // Subscription management.
  void addUrlSubscription(const QString& url, const QString& name, bool useOriginalConfig,
                          int autoUpdateIntervalMinutes, bool applyRuntime, bool enableSharedRules = true,
                          const QStringList& ruleSets = {});
  void addManualSubscription(const QString& content, const QString& name, bool useOriginalConfig, bool isUriList,
                             bool applyRuntime, bool enableSharedRules = true, const QStringList& ruleSets = {});
  void removeSubscription(const QString& id);
  void refreshSubscription(const QString& id, bool applyRuntime);
  void updateAllSubscriptions(bool applyRuntime);
  void updateSubscriptionMeta(const QString& id, const QString& name, const QString& url, bool isManual,
                              const QString& manualContent, bool useOriginalConfig, int autoUpdateIntervalMinutes,
                              bool enableSharedRules, const QStringList& ruleSets);
  void setActiveSubscription(const QString& id, bool applyRuntime);
  void clearActiveSubscription();

  QString getCurrentConfig() const;
  bool    saveCurrentConfig(const QString& content, bool applyRuntime);
  bool    rollbackSubscriptionConfig(const QString& configPath);
  bool    deleteSubscriptionConfig(const QString& configPath);

  // Get subscription list.
  QList<SubscriptionInfo> getSubscriptions() const;
  int                     getActiveIndex() const;
  QString                 getActiveConfigPath() const;

 signals:
  void subscriptionAdded(const SubscriptionInfo& info);
  void subscriptionRemoved(const QString& id);
  void subscriptionUpdated(const QString& id);
  void errorOccurred(const QString& error);
  void activeSubscriptionChanged(const QString& id, const QString& configPath);
  void applyConfigRequested(const QString& configPath, bool restart);
  void progressChanged(const QString& id, int progress);

 private:
  void              saveToDatabase();
  SubscriptionInfo* findSubscription(const QString& id);
  bool              isJsonContent(const QString& content) const;
  void              updateSubscriptionUserinfo(SubscriptionInfo& info, const QJsonObject& headers);
  void              updateSubscriptionUserinfoFromHeader(SubscriptionInfo& info, const QByteArray& header);
  void              syncSharedRulesToConfig(const SubscriptionInfo& info);

  QString generateId() const;

  QList<SubscriptionInfo> m_subscriptions;
  int                     m_activeIndex;
  QString                 m_activeConfigPath;
  ConfigRepository*       m_configRepo = nullptr;
};
#endif  // SUBSCRIPTIONSERVICE_H
