#include "WebSocketClient.h"
#include "utils/Logger.h"

WebSocketClient::WebSocketClient(QObject* parent)
    : QObject(parent),
      m_socket(
          new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this)),
      m_reconnectTimer(new QTimer(this)),
      m_autoReconnect(true),
      m_reconnectInterval(3000),
      m_intentionalDisconnect(false) {
  QObject::connect(
      m_socket, &QWebSocket::connected, this, &WebSocketClient::onConnected);
  QObject::connect(m_socket,
                   &QWebSocket::disconnected,
                   this,
                   &WebSocketClient::onDisconnected);
  QObject::connect(m_socket,
                   &QWebSocket::textMessageReceived,
                   this,
                   &WebSocketClient::onTextMessageReceived);
  QObject::connect(m_socket,
                   &QWebSocket::binaryMessageReceived,
                   this,
                   &WebSocketClient::onBinaryMessageReceived);
  QObject::connect(
      m_socket, &QWebSocket::errorOccurred, this, &WebSocketClient::onError);
  m_reconnectTimer->setSingleShot(true);
  QObject::connect(m_reconnectTimer,
                   &QTimer::timeout,
                   this,
                   &WebSocketClient::attemptReconnect);
}

WebSocketClient::~WebSocketClient() {
  m_intentionalDisconnect = true;
  m_socket->close();
}

void WebSocketClient::connect(const QString& url) {
  m_url                   = url;
  m_intentionalDisconnect = false;
  m_socket->open(QUrl(url));
  Logger::info(QString("WebSocket connect: %1").arg(url));
}

void WebSocketClient::disconnect() {
  m_intentionalDisconnect = true;
  m_reconnectTimer->stop();
  m_socket->close();
}

bool WebSocketClient::isConnected() const {
  return m_socket->state() == QAbstractSocket::ConnectedState;
}

void WebSocketClient::setAutoReconnect(bool enabled) {
  m_autoReconnect = enabled;
}

void WebSocketClient::setReconnectInterval(int msecs) {
  m_reconnectInterval = msecs;
}

void WebSocketClient::onConnected() {
  Logger::info("WebSocket connected");
  emit connected();
}

void WebSocketClient::onDisconnected() {
  Logger::info("WebSocket disconnected");
  emit disconnected();
  if (m_autoReconnect && !m_intentionalDisconnect) {
    m_reconnectTimer->start(m_reconnectInterval);
  }
}

void WebSocketClient::onTextMessageReceived(const QString& message) {
  emit messageReceived(message);
}

void WebSocketClient::onBinaryMessageReceived(const QByteArray& data) {
  emit binaryMessageReceived(data);
}

void WebSocketClient::onError(QAbstractSocket::SocketError error) {
  Q_UNUSED(error)
  QString errorMsg = m_socket->errorString();
  Logger::warn(QString("WebSocket error: %1").arg(errorMsg));
  emit errorOccurred(errorMsg);
}

void WebSocketClient::attemptReconnect() {
  if (!m_intentionalDisconnect && !m_url.isEmpty()) {
    Logger::info("WebSocket reconnecting...");
    m_socket->open(QUrl(m_url));
  }
}
