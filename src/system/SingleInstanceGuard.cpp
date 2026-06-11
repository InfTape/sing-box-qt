#include "SingleInstanceGuard.h"
#include <QElapsedTimer>
#include <QLocalSocket>
#include <QRegularExpression>
#include <QThread>

SingleInstanceGuard::SingleInstanceGuard(const QString& key,
                                         bool waitForReplacement,
                                         QObject* parent)
    : QObject(parent), m_key(normalizeKey(key)) {
  if (!waitForReplacement && canConnect(kConnectTimeoutMs)) {
    m_secondary = true;
    return;
  }

  if (waitForReplacement) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < kReplacementTimeoutMs && canConnect(50)) {
      QThread::msleep(100);
    }

    if (canConnect(50)) {
      m_secondary = true;
      return;
    }
  }

  m_secondary = !startPrimaryServer();
}

bool SingleInstanceGuard::isSecondary() const {
  return m_secondary;
}

void SingleInstanceGuard::notifyPrimary(const QByteArray& message) {
  QLocalSocket socket;
  socket.connectToServer(m_key);
  if (!socket.waitForConnected(kConnectTimeoutMs)) {
    return;
  }

  socket.write(message);
  socket.flush();
  socket.waitForBytesWritten(kConnectTimeoutMs);
  socket.disconnectFromServer();
}

QString SingleInstanceGuard::normalizeKey(const QString& key) {
  QString normalized = key;
  normalized.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")),
                     QStringLiteral("-"));
  return normalized;
}

bool SingleInstanceGuard::canConnect(int timeoutMs) const {
  QLocalSocket socket;
  socket.connectToServer(m_key);
  return socket.waitForConnected(timeoutMs);
}

bool SingleInstanceGuard::startPrimaryServer() {
  QLocalServer::removeServer(m_key);
  if (!m_server.listen(m_key)) {
    return false;
  }

  connect(&m_server,
          &QLocalServer::newConnection,
          this,
          &SingleInstanceGuard::handleNewConnection);
  return true;
}

void SingleInstanceGuard::handleNewConnection() {
  while (QLocalSocket* client = m_server.nextPendingConnection()) {
    connect(client, &QLocalSocket::readyRead, this, [this, client]() {
      handleMessage(client->readAll());
    });
    connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
  }
}

void SingleInstanceGuard::handleMessage(const QByteArray& message) {
  if (message.trimmed() == QByteArray("activate")) {
    emit activationRequested();
  }
}
