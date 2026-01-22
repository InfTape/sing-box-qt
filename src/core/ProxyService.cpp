#include "ProxyService.h"
#include "network/HttpClient.h"
#include "network/WebSocketClient.h"
#include "utils/Logger.h"
#include <QJsonDocument>
#include <QUrlQuery>

ProxyService::ProxyService(QObject *parent)
    : QObject(parent)
    , m_httpClient(new HttpClient(this))
    , m_wsClient(new WebSocketClient(this))
    , m_apiPort(9090)
{
    connect(m_wsClient, &WebSocketClient::messageReceived, this, [this](const QString &message) {
        QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            qint64 up = obj["up"].toVariant().toLongLong();
            qint64 down = obj["down"].toVariant().toLongLong();
            emit trafficUpdated(up, down);
        }
    });
}

ProxyService::~ProxyService()
{
}

void ProxyService::setApiPort(int port)
{
    m_apiPort = port;
}

int ProxyService::getApiPort() const
{
    return m_apiPort;
}

void ProxyService::setApiToken(const QString &token)
{
    m_apiToken = token;
    m_httpClient->setAuthToken(token);
}

QString ProxyService::buildApiUrl(const QString &path) const
{
    return QString("http://127.0.0.1:%1%2").arg(m_apiPort).arg(path);
}

void ProxyService::fetchProxies()
{
    QString url = buildApiUrl("/proxies");
    
    m_httpClient->get(url, [this](bool success, const QByteArray &data) {
        if (success) {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                emit proxiesReceived(doc.object());
            }
        } else {
            emit errorOccurred(tr("获取代理列表失败"));
        }
    });
}

void ProxyService::fetchRules()
{
    QString url = buildApiUrl("/rules");
    
    m_httpClient->get(url, [this](bool success, const QByteArray &data) {
        if (success) {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject() && doc.object().contains("rules")) {
                emit rulesReceived(doc.object()["rules"].toArray());
            }
        } else {
            emit errorOccurred(tr("获取规则列表失败"));
        }
    });
}

void ProxyService::fetchConnections()
{
    QString url = buildApiUrl("/connections");
    
    m_httpClient->get(url, [this](bool success, const QByteArray &data) {
        if (success) {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                emit connectionsReceived(doc.object());
            }
        } else {
            emit errorOccurred(tr("获取连接列表失败"));
        }
    });
}

void ProxyService::selectProxy(const QString &group, const QString &proxy)
{
    QString url = buildApiUrl(QString("/proxies/%1").arg(group));
    
    QJsonObject body;
    body["name"] = proxy;
    
    m_httpClient->put(url, QJsonDocument(body).toJson(), [this, proxy](bool success, const QByteArray &) {
        if (success) {
            Logger::info(QString("已切换代理到: %1").arg(proxy));
        } else {
            emit errorOccurred(tr("切换代理失败"));
        }
    });
}

void ProxyService::testDelay(const QString &proxy, const QString &url, int timeout)
{
    QString testUrl = url.isEmpty() 
        ? "http://www.gstatic.com/generate_204" 
        : url;
    
    QUrlQuery query;
    query.addQueryItem("url", testUrl);
    query.addQueryItem("timeout", QString::number(timeout));
    
    QString apiUrl = buildApiUrl(QString("/proxies/%1/delay?%2")
        .arg(proxy, query.toString()));
    
    m_httpClient->get(apiUrl, [this, proxy](bool success, const QByteArray &data) {
        if (success) {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject() && doc.object().contains("delay")) {
                int delay = doc.object()["delay"].toInt();
                emit delayResult(proxy, delay);
            }
        } else {
            emit delayResult(proxy, -1);
        }
    });
}

void ProxyService::testGroupDelay(const QString &group)
{
    QString url = buildApiUrl(QString("/group/%1/delay").arg(group));
    
    QUrlQuery query;
    query.addQueryItem("url", "http://www.gstatic.com/generate_204");
    query.addQueryItem("timeout", "5000");
    
    m_httpClient->get(url + "?" + query.toString(), [this](bool success, const QByteArray &data) {
        if (success) {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject delays = doc.object();
                for (auto it = delays.begin(); it != delays.end(); ++it) {
                    emit delayResult(it.key(), it.value().toInt());
                }
            }
        } else {
            emit errorOccurred(tr("测试组延迟失败"));
        }
    });
}

void ProxyService::setProxyMode(const QString &mode)
{
    const QString normalized = mode.trimmed().toLower();
    if (normalized.isEmpty()) {
        return;
    }

    QString url = buildApiUrl("/configs");
    QJsonObject body;
    body["mode"] = normalized;

    m_httpClient->put(url, QJsonDocument(body).toJson(), [this, normalized](bool success, const QByteArray &) {
        if (success) {
            Logger::info(QString("已切换代理模式: %1").arg(normalized));
        } else {
            emit errorOccurred(tr("切换代理模式失败"));
        }
    });
}

void ProxyService::closeConnection(const QString &id)
{
    QString url = buildApiUrl(QString("/connections/%1").arg(id));
    
    m_httpClient->del(url, [this](bool success, const QByteArray &) {
        if (!success) {
            emit errorOccurred(tr("关闭连接失败"));
        }
    });
}

void ProxyService::closeAllConnections()
{
    QString url = buildApiUrl("/connections");
    
    m_httpClient->del(url, [this](bool success, const QByteArray &) {
        if (success) {
            Logger::info("已关闭所有连接");
        } else {
            emit errorOccurred(tr("关闭所有连接失败"));
        }
    });
}

void ProxyService::startTrafficMonitor()
{
    QString url = QString("ws://127.0.0.1:%1/traffic").arg(m_apiPort);
    if (!m_apiToken.isEmpty()) {
        url += QString("?token=%1").arg(m_apiToken);
    }
    
    // 断开旧连接（如果有）
    if (m_wsClient->isConnected()) {
        m_wsClient->disconnect();
    }
    
    m_wsClient->connect(url);
}

void ProxyService::stopTrafficMonitor()
{
    m_wsClient->disconnect();
}
