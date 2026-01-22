#ifndef DATABASESERVICE_H
#define DATABASESERVICE_H

#include <QObject>
#include <QSqlDatabase>
#include <QJsonObject>
#include <QJsonArray>

class DatabaseService : public QObject
{
    Q_OBJECT

public:
    static DatabaseService& instance();
    
    bool init();
    void close();
    
    // 应用配置
    QJsonObject getAppConfig();
    bool saveAppConfig(const QJsonObject &config);
    
    // 主题配置
    QJsonObject getThemeConfig();
    bool saveThemeConfig(const QJsonObject &config);
    
    // 语言配置
    QString getLocale();
    bool saveLocale(const QString &locale);
    
    // 订阅数据
    QJsonArray getSubscriptions();
    bool saveSubscriptions(const QJsonArray &subscriptions);
    
    // 活动订阅索引
    int getActiveSubscriptionIndex();
    bool saveActiveSubscriptionIndex(int index);
    QString getActiveConfigPath();
    bool saveActiveConfigPath(const QString &path);

    // 订阅节点存储
    QJsonArray getSubscriptionNodes(const QString &id);
    bool saveSubscriptionNodes(const QString &id, const QJsonArray &nodes);
    
    // 通用键值存储
    QString getValue(const QString &key, const QString &defaultValue = QString());
    bool setValue(const QString &key, const QString &value);

private:
    explicit DatabaseService(QObject *parent = nullptr);
    ~DatabaseService();
    
    bool createTables();
    QString getDatabasePath() const;
    
    QSqlDatabase m_db;
    bool m_initialized;
};

#endif // DATABASESERVICE_H
