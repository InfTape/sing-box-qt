#include "ProxyService.h"
#include <QJsonDocument>
#include <QUrl>
#include "network/HttpClient.h"
#include "network/WebSocketClient.h"
#include "utils/Logger.h"

ProxyService::ProxyService(QObject* parent)
    : QObject(parent), m_httpClient(new HttpClient(this)), m_wsClient(new WebSocketClient(this)), m_apiPort(9090) {
  connect(m_wsClient, &WebSocketClient::messageReceived, this, [this](const QString& message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isObject()) {
      QJsonObject obj  = doc.object();
      qint64      up   = obj["up"].toVariant().toLongLong();
      qint64      down = obj["down"].toVariant().toLongLong();
      emit        trafficUpdated(up, down);
    }
  });
}

ProxyService::~ProxyService() {}

void ProxyService::setApiPort(int port) {
  m_apiPort = port;
}

int ProxyService::getApiPort() const {
  return m_apiPort;
}

void ProxyService::setApiToken(const QString& token) {
  m_apiToken = token;
  m_httpClient->setAuthToken(token);
}

QString ProxyService::getApiToken() const {
  return m_apiToken;
}

QString ProxyService::buildApiUrl(const QString& path) const {
  return QString("http://127.0.0.1:%1%2").arg(m_apiPort).arg(path);
}

void ProxyService::fetchProxies() {
  QString url = buildApiUrl("/proxies");
  m_httpClient->get(url, [this](bool success, const QByteArray& data) {
    if (success) {
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (doc.isObject()) {
        emit proxiesReceived(doc.object());
      }
    } else {
      emit errorOccurred(tr("Failed to fetch proxies"));
    }
  });
}

void ProxyService::fetchRules() {
  QString url = buildApiUrl("/rules");
  m_httpClient->get(url, [this](bool success, const QByteArray& data) {
    if (success) {
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (doc.isObject() && doc.object().contains("rules")) {
        emit rulesReceived(doc.object()["rules"].toArray());
      }
    } else {
      emit errorOccurred(tr("Failed to fetch rules"));
    }
  });
}

void ProxyService::fetchConnections() {
  QString url = buildApiUrl("/connections");
  m_httpClient->get(url, [this](bool success, const QByteArray& data) {
    if (success) {
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (doc.isObject()) {
        emit connectionsReceived(doc.object());
      }
    } else {
      emit errorOccurred(tr("Failed to fetch connections"));
    }
  });
}

void ProxyService::selectProxy(const QString& group, const QString& proxy) {
  const QString groupPath = QString::fromUtf8(QUrl::toPercentEncoding(group));
  QString       url       = buildApiUrl(QString("/proxies/%1").arg(groupPath));
  QJsonObject   body;
  body["name"] = proxy;
  m_httpClient->put(url, QJsonDocument(body).toJson(), [this, group, proxy](bool success, const QByteArray&) {
    if (success) {
      Logger::info(QString("Proxy switched to: %1").arg(proxy));
      emit proxySelected(group, proxy);
    } else {
      Logger::warn(QString("Proxy switch failed: group=%1, proxy=%2").arg(group, proxy));
      emit proxySelectFailed(group, proxy);
      emit errorOccurred(tr("Failed to switch proxy"));
    }
  });
}

void ProxyService::setProxyMode(const QString& mode) {
  const QString normalized = mode.trimmed().toLower();
  if (normalized.isEmpty()) {
    return;
  }
  QString     url = buildApiUrl("/configs");
  QJsonObject body;
  body["mode"] = normalized;
  m_httpClient->put(url, QJsonDocument(body).toJson(), [this, normalized](bool success, const QByteArray&) {
    if (success) {
      Logger::info(QString("Proxy mode switched: %1").arg(normalized));
    } else {
      emit errorOccurred(tr("Failed to switch proxy mode"));
    }
  });
}

void ProxyService::closeConnection(const QString& id) {
  QString url = buildApiUrl(QString("/connections/%1").arg(id));
  m_httpClient->del(url, [this](bool success, const QByteArray&) {
    if (!success) {
      emit errorOccurred(tr("Failed to close connection"));
    }
  });
}

void ProxyService::closeAllConnections() {
  QString url = buildApiUrl("/connections");
  m_httpClient->del(url, [this](bool success, const QByteArray&) {
    if (success) {
      Logger::info("Closed all connections");
    } else {
      emit errorOccurred(tr("Failed to close all connections"));
    }
  });
}

void ProxyService::startTrafficMonitor() {
  QString url = QString("ws://127.0.0.1:%1/traffic").arg(m_apiPort);
  if (!m_apiToken.isEmpty()) {
    url += QString("?token=%1").arg(m_apiToken);
  }
  // Disconnect old connection if any.
  if (m_wsClient->isConnected()) {
    m_wsClient->disconnect();
  }
  m_wsClient->connect(url);
}

void ProxyService::stopTrafficMonitor() {
  m_wsClient->disconnect();
}
