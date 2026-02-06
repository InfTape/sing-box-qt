#ifndef DATABASESERVICE_H
#define DATABASESERVICE_H
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QSqlDatabase>
class DatabaseService : public QObject {
  Q_OBJECT
 public:
  static DatabaseService& instance();
  bool                    init();
  void                    close();
  // Application config.
  QJsonObject getAppConfig();
  bool        saveAppConfig(const QJsonObject& config);
  // Theme config.
  QJsonObject getThemeConfig();
  bool        saveThemeConfig(const QJsonObject& config);
  // Locale config.
  QString getLocale();
  bool    saveLocale(const QString& locale);
  // Subscription data.
  QJsonArray getSubscriptions();
  bool       saveSubscriptions(const QJsonArray& subscriptions);
  // Active subscription index.
  int     getActiveSubscriptionIndex();
  bool    saveActiveSubscriptionIndex(int index);
  QString getActiveConfigPath();
  bool    saveActiveConfigPath(const QString& path);
  // Subscription nodes storage.
  QJsonArray getSubscriptionNodes(const QString& id);
  bool       saveSubscriptionNodes(const QString& id, const QJsonArray& nodes);
  // Generic key/value store.
  QString getValue(const QString& key, const QString& defaultValue = QString());
  bool    setValue(const QString& key, const QString& value);
  // Data usage persistence (JSON payload).
  QJsonObject getDataUsage();
  bool        saveDataUsage(const QJsonObject& payload);
  bool        clearDataUsage();

 private:
  explicit DatabaseService(QObject* parent = nullptr);
  ~DatabaseService();
  bool         createTables();
  QString      getDatabasePath() const;
  QSqlDatabase m_db;
  bool         m_initialized;
};
#endif  // DATABASESERVICE_H
