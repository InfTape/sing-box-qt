#ifndef SINGLEINSTANCEGUARD_H
#define SINGLEINSTANCEGUARD_H

#include <QByteArray>
#include <QLocalServer>
#include <QObject>
#include <QString>

class SingleInstanceGuard : public QObject {
  Q_OBJECT

 public:
  explicit SingleInstanceGuard(const QString& key,
                               bool           waitForReplacement = false,
                               QObject*       parent             = nullptr);

  bool isSecondary() const;
  void notifyPrimary(const QByteArray& message = QByteArray("activate"));

 signals:
  void activationRequested();

 private:
  static constexpr int kConnectTimeoutMs     = 150;
  static constexpr int kReplacementTimeoutMs = 5000;

  static QString normalizeKey(const QString& key);

  bool canConnect(int timeoutMs) const;
  bool startPrimaryServer();
  void handleNewConnection();
  void handleMessage(const QByteArray& message);

  QString      m_key;
  bool         m_secondary = false;
  QLocalServer m_server;
};

#endif  // SINGLEINSTANCEGUARD_H
