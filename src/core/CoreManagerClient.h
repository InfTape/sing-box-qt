#ifndef COREMANAGERCLIENT_H
#define COREMANAGERCLIENT_H

#include <QObject>
#include <QJsonObject>
#include <QLocalSocket>

class CoreManagerClient : public QObject {
  Q_OBJECT

 public:
  explicit CoreManagerClient(QObject* parent = nullptr);

  void connectToServer(const QString& name);
  void disconnectFromServer();
  bool isConnected() const;
  bool waitForConnected(int timeoutMs);
  void abort();

  void sendRequest(int id, const QString& method, const QJsonObject& params);

 signals:
  void connected();
  void disconnected();
  void responseReceived(int id, bool ok, const QJsonObject& result,
                        const QString& error);
  void statusEvent(bool running);
  void logEvent(const QString& stream, const QString& message);
  void errorEvent(const QString& message);

 private slots:
  void onReadyRead();
  void onDisconnected();

 private:
  void handleMessage(const QJsonObject& obj);

  QLocalSocket* m_socket;
  QByteArray    m_buffer;
};

#endif  // COREMANAGERCLIENT_H
