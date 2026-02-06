#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H
#include <QObject>
#include <QTimer>
#include <QWebSocket>

class WebSocketClient : public QObject {
  Q_OBJECT
 public:
  explicit WebSocketClient(QObject* parent = nullptr);
  ~WebSocketClient();
  void connect(const QString& url);
  void disconnect();
  bool isConnected() const;
  void setAutoReconnect(bool enabled);
  void setReconnectInterval(int msecs);
 signals:
  void connected();
  void disconnected();
  void messageReceived(const QString& message);
  void binaryMessageReceived(const QByteArray& data);
  void errorOccurred(const QString& error);
 private slots:
  void onConnected();
  void onDisconnected();
  void onTextMessageReceived(const QString& message);
  void onBinaryMessageReceived(const QByteArray& data);
  void onError(QAbstractSocket::SocketError error);
  void attemptReconnect();

 private:
  QWebSocket* m_socket;
  QTimer*     m_reconnectTimer;
  QString     m_url;
  bool        m_autoReconnect;
  int         m_reconnectInterval;
  bool        m_intentionalDisconnect;
};
#endif  // WEBSOCKETCLIENT_H
