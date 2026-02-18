#include "CoreManagerClient.h"
#include <QJsonDocument>
#include <QJsonValue>

namespace {
constexpr int kMaxIpcBufferBytes = 1024 * 1024;
}  // namespace

CoreManagerClient::CoreManagerClient(QObject* parent)
    : QObject(parent), m_socket(new QLocalSocket(this)) {
  connect(m_socket,
          &QLocalSocket::readyRead,
          this,
          &CoreManagerClient::onReadyRead);
  connect(
      m_socket, &QLocalSocket::connected, this, &CoreManagerClient::connected);
  connect(m_socket,
          &QLocalSocket::disconnected,
          this,
          &CoreManagerClient::onDisconnected);
}

void CoreManagerClient::connectToServer(const QString& name) {
  if (m_socket->state() == QLocalSocket::ConnectedState ||
      m_socket->state() == QLocalSocket::ConnectingState) {
    return;
  }
  m_socket->connectToServer(name);
}

void CoreManagerClient::disconnectFromServer() {
  if (m_socket->state() != QLocalSocket::UnconnectedState) {
    m_socket->disconnectFromServer();
  }
}

bool CoreManagerClient::isConnected() const {
  return m_socket->state() == QLocalSocket::ConnectedState;
}

bool CoreManagerClient::waitForConnected(int timeoutMs) {
  return m_socket->waitForConnected(timeoutMs);
}

void CoreManagerClient::abort() {
  m_socket->abort();
}

void CoreManagerClient::sendRequest(int                id,
                                    const QString&     method,
                                    const QJsonObject& params) {
  QJsonObject obj;
  obj["id"]     = id;
  obj["method"] = method;
  if (!params.isEmpty()) {
    obj["params"] = params;
  }
  const QByteArray payload =
      QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
  m_socket->write(payload);
  m_socket->flush();
}

void CoreManagerClient::onReadyRead() {
  m_buffer.append(m_socket->readAll());
  if (m_buffer.size() > kMaxIpcBufferBytes) {
    m_buffer.clear();
    m_socket->abort();
    emit errorEvent(tr("Core manager IPC message too large"));
    return;
  }
  while (true) {
    const int idx = m_buffer.indexOf('\n');
    if (idx < 0) {
      break;
    }
    QByteArray line = m_buffer.left(idx);
    m_buffer.remove(0, idx + 1);
    line = line.trimmed();
    if (line.isEmpty()) {
      continue;
    }
    QJsonParseError err;
    QJsonDocument   doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
      continue;
    }
    handleMessage(doc.object());
  }
}

void CoreManagerClient::onDisconnected() {
  m_buffer.clear();
  emit disconnected();
}

void CoreManagerClient::handleMessage(const QJsonObject& obj) {
  const QString event = obj.value("event").toString();
  if (!event.isEmpty()) {
    if (event == "status") {
      emit statusEvent(obj.value("running").toBool());
    } else if (event == "log") {
      emit logEvent(obj.value("stream").toString(),
                    obj.value("message").toString());
    } else if (event == "error") {
      emit errorEvent(obj.value("message").toString());
    }
    return;
  }
  if (!obj.contains("id")) {
    return;
  }
  const int     id    = obj.value("id").toInt(-1);
  const bool    ok    = obj.value("ok").toBool(false);
  const QString error = obj.value("error").toString();
  QJsonObject   result;
  if (obj.value("result").isObject()) {
    result = obj.value("result").toObject();
  }
  emit responseReceived(id, ok, result, error);
}
