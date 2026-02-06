#ifndef COREMANAGERSERVER_H
#define COREMANAGERSERVER_H
#include <QObject>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
class KernelRunner;
class CoreManagerServer : public QObject {
  Q_OBJECT
 public:
  explicit CoreManagerServer(QObject* parent = nullptr);
  bool    startListening(const QString& name, QString* error = nullptr);
  QString serverName() const;
 private slots:
  void onNewConnection();
  void onReadyRead();
  void onClientDisconnected();
  void onKernelStatusChanged(bool running);
  void onKernelOutput(const QString& output);
  void onKernelErrorOutput(const QString& output);
  void onKernelError(const QString& error);

 private:
  void          handleMessage(const QJsonObject& obj);
  void          sendResponse(int id, bool ok, const QJsonObject& result, const QString& error);
  void          sendEvent(const QJsonObject& event);
  QLocalServer* m_server;
  QLocalSocket* m_client;
  QByteArray    m_buffer;
  KernelRunner* m_kernel;
  QString       m_serverName;
};
#endif  // COREMANAGERSERVER_H
