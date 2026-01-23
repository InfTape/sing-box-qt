#ifndef PROXYSERVICE_H
#define PROXYSERVICE_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

class HttpClient;
class WebSocketClient;

class ProxyService : public QObject
{
    Q_OBJECT

public:
    explicit ProxyService(QObject *parent = nullptr);
    ~ProxyService();

    // API port config.
    void setApiPort(int port);
    int getApiPort() const;
    void setApiToken(const QString &token);

    // Fetch proxy data.
    void fetchProxies();
    void fetchRules();
    void fetchConnections();

    // Proxy operations.
    void selectProxy(const QString &group, const QString &proxy);
    void testDelay(const QString &proxy, const QString &url = QString(), int timeout = 5000);
    void testGroupDelay(const QString &group);
    void setProxyMode(const QString &mode);

    // Connection operations.
    void closeConnection(const QString &id);
    void closeAllConnections();

    void startTrafficMonitor();
    void stopTrafficMonitor();

signals:
    void proxiesReceived(const QJsonObject &proxies);
    void rulesReceived(const QJsonArray &rules);
    void connectionsReceived(const QJsonObject &connections);
    void delayResult(const QString &proxy, int delay);
    void errorOccurred(const QString &error);
    void trafficUpdated(qint64 up, qint64 down);

private:
    QString buildApiUrl(const QString &path) const;

    HttpClient *m_httpClient;
    WebSocketClient *m_wsClient;
    int m_apiPort;
    QString m_apiToken;
};

#endif // PROXYSERVICE_H
