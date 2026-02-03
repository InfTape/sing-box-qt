#ifndef HOMEVIEW_H
#define HOMEVIEW_H

#include <QElapsedTimer>
#include <QJsonObject>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

class QCheckBox;
class QPushButton;
class TrafficChart;
class ToggleSwitch;
class SegmentedControl;
class ThemeService;
class HomeView : public QWidget {
  Q_OBJECT

 public:
  explicit HomeView(ThemeService* themeService, QWidget* parent = nullptr);
  bool isSystemProxyEnabled() const;
  void setSystemProxyEnabled(bool enabled);
  bool isTunModeEnabled() const;
  void setTunModeEnabled(bool enabled);
  void setProxyMode(const QString& mode);

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

 private slots:
  void onSystemProxyToggled(bool checked);
  void onTunModeToggled(bool checked);
  void onGlobalModeClicked();
  void onRuleModeClicked();

 private:
  void     setupUI();
  void     setCardActive(QWidget* card, bool active);
  QString  formatDuration(int seconds) const;
  QString  formatTimeRange(qint64 firstMs, qint64 lastMs) const;
  void     refreshDataUsageTable();
  QWidget* createStatCard(const QString& iconText, const QString& accentKey,
                          const QString& title, QLabel** valueLabel,
                          QLabel** subLabel);
  QWidget* createModeItem(const QString& iconText, const QString& accentKey,
                          const QString& title, const QString& desc,
                          QWidget* control);

  QWidget* m_statusBadge = nullptr;
  QWidget* m_statusDot   = nullptr;
  QLabel*  m_statusText  = nullptr;

  QLabel* m_uploadValue      = nullptr;
  QLabel* m_uploadTotal      = nullptr;
  QLabel* m_downloadValue    = nullptr;
  QLabel* m_downloadTotal    = nullptr;
  QLabel* m_connectionsValue = nullptr;
  QLabel* m_memoryLabel      = nullptr;

  TrafficChart* m_trafficChart = nullptr;

  QWidget* m_systemProxyCard = nullptr;
  QWidget* m_tunModeCard     = nullptr;
  QWidget* m_globalModeCard  = nullptr;
  QWidget* m_ruleModeCard    = nullptr;

  ToggleSwitch* m_systemProxySwitch = nullptr;
  ToggleSwitch* m_tunModeSwitch     = nullptr;
  ToggleSwitch* m_globalModeSwitch  = nullptr;
  ToggleSwitch* m_ruleModeSwitch    = nullptr;


  SegmentedControl*  m_rankingModeSelector  = nullptr;
  QPushButton*       m_dataUsageClearBtn    = nullptr;
  QTableWidget*      m_dataUsageTopTable    = nullptr;
  QLabel*            m_dataUsageEmpty       = nullptr;
  QJsonObject   m_dataUsageSnapshot;

  bool          m_isRunning     = false;
  qint64        m_totalUpload   = 0;
  qint64        m_totalDownload = 0;
  QElapsedTimer m_trafficTimer;
  ThemeService* m_themeService = nullptr;
};
#endif  // HOMEVIEW_H
