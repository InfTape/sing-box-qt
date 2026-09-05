#ifndef COREDATASERVICE_H
#define COREDATASERVICE_H

#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QTimer;
class DataUsageTracker;
class LogStore;

class CoreDataService : public QObject {
  Q_OBJECT
 public:
  explicit CoreDataService(QObject* parent = nullptr);
  ~CoreDataService() override;

  void start(const QString& configPath);
  void stop();
  bool isRunning() const { return m_running; }

  void appendKernelOutput(const QString& stream, const QString& message);

  QJsonObject dataUsageSnapshot(int limit = 50) const;
  void        resetDataUsage();

 signals:
  void dataUsageUpdated(const QJsonObject& snapshot);
  void apiLogMessage(const QString& type, const QString& payload);

 private slots:
  void pollConnections();

 private:
  void parseConfigApi(const QString& configPath);

  DataUsageTracker*      m_tracker         = nullptr;
  LogStore*              m_logStore        = nullptr;
  QNetworkAccessManager* m_nam             = nullptr;
  QTimer*                m_connectionTimer = nullptr;
  QString                m_configPath;
  QString                m_apiToken;
  int                    m_apiPort = 9090;
  bool                   m_running = false;
};

#endif  // COREDATASERVICE_H
