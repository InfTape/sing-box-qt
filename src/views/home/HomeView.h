#ifndef HOMEVIEW_H
#define HOMEVIEW_H
#include <QElapsedTimer>
#include <QJsonObject>
#include <QLabel>
#include <QWidget>
class DataUsageCard;
class ProxyModeSection;
class StatCard;
class TrafficChart;
class ThemeService;
class HomeView : public QWidget {
  Q_OBJECT
 public:
  explicit HomeView(ThemeService* themeService, QWidget* parent = nullptr);
  bool    isSystemProxyEnabled() const;
  void    setSystemProxyEnabled(bool enabled);
  bool    isTunModeEnabled() const;
  void    setTunModeEnabled(bool enabled);
  void    setProxyMode(const QString& mode);
  void    updateStatus(bool running);
  void    updateTraffic(qint64 upload, qint64 download);
  void    updateUptime(int seconds);
  void    updateConnections(int count, qint64 memoryUsage);
  void    updateDataUsage(const QJsonObject& snapshot);
  QString formatBytes(qint64 bytes) const;
 signals:
  void systemProxyChanged(bool enabled);
  void tunModeChanged(bool enabled);
  void proxyModeChanged(const QString& mode);
  void dataUsageClearRequested();
 public slots:
  void updateStyle();

 private:
  void              setupUI();
  QString           formatDuration(int seconds) const;
  QWidget*          m_statusBadge      = nullptr;
  QWidget*          m_statusDot        = nullptr;
  QLabel*           m_statusText       = nullptr;
  StatCard*         m_uploadCard       = nullptr;
  StatCard*         m_downloadCard     = nullptr;
  StatCard*         m_connectionsCard  = nullptr;
  TrafficChart*     m_trafficChart     = nullptr;
  ProxyModeSection* m_proxyModeSection = nullptr;
  DataUsageCard*    m_dataUsageCard    = nullptr;
  bool              m_isRunning        = false;
  qint64            m_totalUpload      = 0;
  qint64            m_totalDownload    = 0;
  QElapsedTimer     m_trafficTimer;
  ThemeService*     m_themeService = nullptr;
};
#endif  // HOMEVIEW_H
