#ifndef KERNELSERVICE_H
#define KERNELSERVICE_H

#include <QObject>
#include <QProcess>
#include <QString>
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
  void onProcessStarted();
  void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
  void onProcessError(QProcess::ProcessError error);
  void onReadyReadStandardOutput();
  void onReadyReadStandardError();

 private:
  QString findKernelPath() const;
  QString getDefaultConfigPath() const;

  QProcess* m_process;
  QString   m_kernelPath;
  QString   m_configPath;
  bool      m_running;
  bool      m_stopRequested  = false;
  bool      m_restartPending = false;
};
#endif  // KERNELSERVICE_H
