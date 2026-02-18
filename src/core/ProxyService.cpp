#include "ProxyService.h"
#include <QJsonDocument>
#include <QSet>
#include <QUrl>
#include "network/HttpClient.h"
#include "network/WebSocketClient.h"
#include "utils/Logger.h"

namespace {
bool isPolicyGroupType(const QString& type) {
  const QString normalized = type.trimmed().toLower();
  return normalized == "selector" || normalized == "urltest" ||
         normalized == "fallback";
}

QHash<QString, QString> extractGroupNowCache(const QJsonObject& payload) {
  QHash<QString, QString> cache;
  const QJsonObject       proxies = payload.value("proxies").toObject();
  for (auto it = proxies.begin(); it != proxies.end(); ++it) {
    if (!it.value().isObject()) {
      continue;
    }
    const QJsonObject proxy = it.value().toObject();
    if (!isPolicyGroupType(proxy.value("type").toString())) {
      continue;
    }
    const QString group = it.key().trimmed();
    const QString now   = proxy.value("now").toString().trimmed();
    if (!group.isEmpty() && !now.isEmpty()) {
      cache.insert(group, now);
    }
  }
  return cache;
}
}  // namespace

ProxyService::ProxyService(QObject* parent)
    : QObject(parent),
      m_httpClient(new HttpClient(this)),
      m_wsClient(new WebSocketClient(this)),
      m_apiPort(9090) {
  connect(m_wsClient,
          &WebSocketClient::messageReceived,
          this,
          [this](const QString& message) {
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

QHash<QString, QString> ProxyService::groupNowCache() const {
  return m_groupNowCache;
}

QString ProxyService::resolveGroupNow(const QString& group) const {
  QString current = group.trimmed();
  if (current.isEmpty()) {
    return current;
  }
  QSet<QString> visited;
  while (!current.isEmpty() && m_groupNowCache.contains(current)) {
    if (visited.contains(current)) {
      break;
    }
    visited.insert(current);
    const QString next = m_groupNowCache.value(current).trimmed();
    if (next.isEmpty() || next.compare(current, Qt::CaseInsensitive) == 0) {
      break;
    }
    current = next;
  }
  return current;
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
        const QJsonObject payload = doc.object();
        m_groupNowCache           = extractGroupNowCache(payload);
        emit proxiesReceived(payload);
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
  if (m_connectionsInFlight) {
    return;
  }
  m_connectionsInFlight = true;
  QString url = buildApiUrl("/connections");
  m_httpClient->get(url, [this](bool success, const QByteArray& data) {
    m_connectionsInFlight = false;
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
  m_httpClient->put(
      url,
      QJsonDocument(body).toJson(),
      [this, group, proxy](bool success, const QByteArray&) {
        if (success) {
          Logger::info(QString("Proxy switched to: %1").arg(proxy));
          const QString groupTag = group.trimmed();
          const QString nodeTag  = proxy.trimmed();
          if (!groupTag.isEmpty() && !nodeTag.isEmpty()) {
            m_groupNowCache.insert(groupTag, nodeTag);
          }
          emit proxySelected(group, proxy);
        } else {
          Logger::warn(QString("Proxy switch failed: group=%1, proxy=%2")
                           .arg(group, proxy));
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
  m_httpClient->put(
      url,
      QJsonDocument(body).toJson(),
      [this, normalized](bool success, const QByteArray&) {
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
