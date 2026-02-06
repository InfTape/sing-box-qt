#include "CoreManagerServer.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QTimer>

#include "coremanager/KernelRunner.h"
#include "utils/Logger.h"

CoreManagerServer::CoreManagerServer(QObject* parent)
    : QObject(parent), m_server(new QLocalServer(this)), m_client(nullptr), m_kernel(new KernelRunner(this)) {
  connect(m_server, &QLocalServer::newConnection, this, &CoreManagerServer::onNewConnection);

  connect(m_kernel, &KernelRunner::statusChanged, this, &CoreManagerServer::onKernelStatusChanged);
  connect(m_kernel, &KernelRunner::outputReceived, this, &CoreManagerServer::onKernelOutput);
  connect(m_kernel, &KernelRunner::errorOutputReceived, this, &CoreManagerServer::onKernelErrorOutput);
  connect(m_kernel, &KernelRunner::errorOccurred, this, &CoreManagerServer::onKernelError);
}

bool CoreManagerServer::startListening(const QString& name, QString* error) {
  if (m_server->isListening()) {
    m_server->close();
  }
  m_serverName = name;

  QLocalServer::removeServer(name);
  if (!m_server->listen(name)) {
    if (error) {
      *error = QString("Failed to listen on %1: %2").arg(name, m_server->errorString());
    }
    return false;
  }
  Logger::info(QString("Core manager listening: %1").arg(name));
  return true;
}

QString CoreManagerServer::serverName() const {
  return m_serverName;
}

void CoreManagerServer::onNewConnection() {
  QLocalSocket* socket = m_server->nextPendingConnection();
  if (!socket) return;

  if (m_client) {
    m_client->disconnectFromServer();
    m_client->deleteLater();
  }
  m_client = socket;

  connect(m_client, &QLocalSocket::readyRead, this, &CoreManagerServer::onReadyRead);
  connect(m_client, &QLocalSocket::disconnected, this, &CoreManagerServer::onClientDisconnected);

  QJsonObject event;
  event["event"]   = "status";
  event["running"] = m_kernel->isRunning();
  sendEvent(event);
}

void CoreManagerServer::onReadyRead() {
  if (!m_client) return;
  m_buffer.append(m_client->readAll());
  while (true) {
    const int idx = m_buffer.indexOf('\n');
    if (idx < 0) break;
    QByteArray line = m_buffer.left(idx);
    m_buffer.remove(0, idx + 1);
    line = line.trimmed();
    if (line.isEmpty()) continue;
    QJsonParseError err;
    QJsonDocument   doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
      continue;
    }
    handleMessage(doc.object());
  }
}

void CoreManagerServer::onClientDisconnected() {
  m_client = nullptr;
}

void CoreManagerServer::onKernelStatusChanged(bool running) {
  QJsonObject event;
  event["event"]   = "status";
  event["running"] = running;
  sendEvent(event);
}

void CoreManagerServer::onKernelOutput(const QString& output) {
  QJsonObject event;
  event["event"]   = "log";
  event["stream"]  = "stdout";
  event["message"] = output;
  sendEvent(event);
}

void CoreManagerServer::onKernelErrorOutput(const QString& output) {
  QJsonObject event;
  event["event"]   = "log";
  event["stream"]  = "stderr";
  event["message"] = output;
  sendEvent(event);
}

void CoreManagerServer::onKernelError(const QString& error) {
  QJsonObject event;
  event["event"]   = "error";
  event["message"] = error;
  sendEvent(event);
}

void CoreManagerServer::handleMessage(const QJsonObject& obj) {
  const int         id     = obj.value("id").toInt(-1);
  const QString     method = obj.value("method").toString();
  const QJsonObject params = obj.value("params").toObject();

  if (method == "start") {
    const QString configPath = params.value("configPath").toString();
    const bool    ok         = m_kernel->start(configPath);
    QJsonObject   result;
    result["running"] = m_kernel->isRunning();
    sendResponse(id, ok, result, ok ? QString() : m_kernel->lastError());
    return;
  }

  if (method == "stop") {
    m_kernel->stop();
    QJsonObject result;
    result["running"] = m_kernel->isRunning();
    sendResponse(id, true, result, QString());
    return;
  }

  if (method == "restart") {
    const QString configPath = params.value("configPath").toString();
    if (!configPath.isEmpty()) {
      m_kernel->restartWithConfig(configPath);
    } else {
      m_kernel->restart();
    }
    QJsonObject result;
    result["running"] = m_kernel->isRunning();
    sendResponse(id, true, result, QString());
    return;
  }

  if (method == "status") {
    QJsonObject result;
    result["running"] = m_kernel->isRunning();
    sendResponse(id, true, result, QString());
    return;
  }

  if (method == "setConfig") {
    const QString configPath = params.value("configPath").toString();
    if (!configPath.isEmpty()) {
      m_kernel->setConfigPath(configPath);
    }
    QJsonObject result;
    result["configPath"] = m_kernel->getConfigPath();
    sendResponse(id, true, result, QString());
    return;
  }

  if (method == "shutdown") {
    sendResponse(id, true, QJsonObject(), QString());
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
    return;
  }

  sendResponse(id, false, QJsonObject(), QString("Unknown method: %1").arg(method));
}

void CoreManagerServer::sendResponse(int id, bool ok, const QJsonObject& result, const QString& error) {
  if (!m_client || id < 0) return;
  QJsonObject obj;
  obj["id"] = id;
  obj["ok"] = ok;
  if (!result.isEmpty()) {
    obj["result"] = result;
  }
  if (!error.isEmpty()) {
    obj["error"] = error;
  }
  const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
  m_client->write(payload);
  m_client->flush();
}

void CoreManagerServer::sendEvent(const QJsonObject& event) {
  if (!m_client) return;
  const QByteArray payload = QJsonDocument(event).toJson(QJsonDocument::Compact) + "\n";
  m_client->write(payload);
  m_client->flush();
}
