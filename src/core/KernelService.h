#ifndef KERNELSERVICE_H
#define KERNELSERVICE_H
#include <QObject>
#include <QString>
#include <QJsonObject>
class QProcess;
class CoreManagerClient;

class KernelService : public QObject {
  Q_OBJECT
 public:
  explicit KernelService(QObject* parent = nullptr);
  ~KernelService();
  bool    start(const QString& configPath = QString());
  void    stop();
  void    restart();
  void    restartWithConfig(const QString& configPath);
  void    setConfigPath(const QString& configPath);
  QString getConfigPath() const;
  bool    isRunning() const;
  QString getVersion() const;
  QString getKernelPath() const;
 signals:
  void statusChanged(bool running);
  void outputReceived(const QString& output);
  void errorOccurred(const QString& error);
 private slots:
  void onManagerStatus(bool running);
  void onManagerLog(const QString& stream, const QString& message);
  void onManagerError(const QString& error);
  void onManagerDisconnected();

 private:
  bool    ensureManagerReady(QString* error = nullptr);
  bool    sendRequestAndWait(const QString& method, const QJsonObject& params, QJsonObject* result, QString* error);
  QString findKernelPath() const;
  QString getDefaultConfigPath() const;
  QString findCoreManagerPath() const;
  CoreManagerClient* m_client;
  QProcess*          m_managerProcess;
  QString            m_serverName;
  QString            m_kernelPath;
  QString            m_configPath;
  bool               m_running;
  bool               m_spawnedManager = false;
  int                m_nextRequestId  = 1;
};
#endif  // KERNELSERVICE_H
