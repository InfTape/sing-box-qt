#include "HomeView.h"
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMap>
#include <QStyle>
#include "app/interfaces/ThemeService.h"
#include "utils/home/HomeFormat.h"
#include "views/components/DataUsageCard.h"
#include "views/components/ProxyModeSection.h"
#include "views/components/StatCard.h"
#include "views/components/TrafficChart.h"

namespace {
QString rgba(const QColor& color, double alpha) {
  return QString("rgba(%1, %2, %3, %4)")
      .arg(color.red())
      .arg(color.green())
      .arg(color.blue())
      .arg(alpha);
}

void polishWidget(QWidget* widget) {
  if (!widget) {
    return;
  }
  widget->style()->unpolish(widget);
  widget->style()->polish(widget);
  widget->update();
}
}  // namespace

HomeView::HomeView(ThemeService* themeService, QWidget* parent)
    : QWidget(parent), m_themeService(themeService) {
  setupUI();
  updateStyle();
  if (m_themeService) {
    connect(m_themeService,
            &ThemeService::themeChanged,
            this,
            &HomeView::updateStyle);
  }
}

void HomeView::setupUI() {
  QHBoxLayout* rootLayout = new QHBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  QWidget* pageContainer = new QWidget;
  pageContainer->setObjectName("PageContainer");
  pageContainer->setMaximumWidth(1200);
  QVBoxLayout* mainLayout = new QVBoxLayout(pageContainer);
  mainLayout->setContentsMargins(24, 16, 24, 16);
  mainLayout->setSpacing(20);
  rootLayout->addStretch();
  rootLayout->addWidget(pageContainer, 1);
  rootLayout->addStretch();
  rootLayout->setAlignment(pageContainer, Qt::AlignTop);
  // Header
  QWidget*     headerWidget = new QWidget;
  QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
  headerLayout->setContentsMargins(0, 0, 0, 0);
  headerLayout->setSpacing(12);
  QLabel* titleLabel = new QLabel(tr("Home"));
  titleLabel->setObjectName("PageTitle");
  m_statusBadge = new QWidget;
  m_statusBadge->setObjectName("StatusBadge");
  m_statusBadge->setProperty("status", "stopped");
  QHBoxLayout* badgeLayout = new QHBoxLayout(m_statusBadge);
  badgeLayout->setContentsMargins(8, 4, 8, 4);
  badgeLayout->setSpacing(6);
  m_statusDot = new QWidget;
  m_statusDot->setObjectName("StatusDot");
  m_statusDot->setFixedSize(8, 8);
  m_statusDot->setProperty("status", "stopped");
  m_statusText = new QLabel(tr("Stopped"));
  m_statusText->setObjectName("StatusText");
  badgeLayout->addWidget(m_statusDot);
  badgeLayout->addWidget(m_statusText);
  headerLayout->addWidget(titleLabel);
  headerLayout->addWidget(m_statusBadge);
  headerLayout->addStretch();
  mainLayout->addWidget(headerWidget);
  // Main grid
  QGridLayout* gridLayout = new QGridLayout;
  gridLayout->setHorizontalSpacing(24);
  gridLayout->setVerticalSpacing(24);
  QWidget*     statsSection = new QWidget;
  QVBoxLayout* statsLayout  = new QVBoxLayout(statsSection);
  statsLayout->setContentsMargins(0, 0, 0, 0);
  statsLayout->setSpacing(24);
  QGridLayout* statsRow = new QGridLayout;
  statsRow->setHorizontalSpacing(24);
  statsRow->setVerticalSpacing(24);
  m_uploadCard =
      new StatCard("UP", "success", tr("Upload"), m_themeService, this);
  m_downloadCard =
      new StatCard("DOWN", "primary", tr("Download"), m_themeService, this);
  m_connectionsCard =
      new StatCard("CONN", "warning", tr("Connections"), m_themeService, this);
  m_uploadCard->setValueText(tr("0 B/s"));
  m_downloadCard->setValueText(tr("0 B/s"));
  m_connectionsCard->setValueText("0");
  m_uploadCard->setSubText(tr("Total: 0 B"));
  m_downloadCard->setSubText(tr("Total: 0 B"));
  m_connectionsCard->setSubText(tr("Memory usage: 0 B"));
  statsRow->addWidget(m_uploadCard, 0, 0);
  statsRow->addWidget(m_downloadCard, 0, 1);
  statsRow->addWidget(m_connectionsCard, 0, 2);
  statsRow->setColumnStretch(0, 1);
  statsRow->setColumnStretch(1, 1);
  statsRow->setColumnStretch(2, 1);
  statsLayout->addLayout(statsRow);
  // Traffic chart + Data usage (same row)
  QHBoxLayout* chartsRow = new QHBoxLayout;
  chartsRow->setSpacing(24);
  QFrame* chartCard = new QFrame;
  chartCard->setObjectName("ChartCard");
  QVBoxLayout* chartLayout = new QVBoxLayout(chartCard);
  chartLayout->setContentsMargins(12, 12, 12, 12);
  chartLayout->setSpacing(0);
  m_trafficChart = new TrafficChart(m_themeService, this);
  m_trafficChart->setFixedHeight(150);
  chartLayout->addWidget(m_trafficChart);
  chartsRow->addWidget(chartCard, 1);
  gridLayout->addWidget(statsSection, 0, 0, 1, 2);
  m_proxyModeSection = new ProxyModeSection(m_themeService, this);
  gridLayout->addWidget(m_proxyModeSection, 1, 0, 1, 2);
  m_dataUsageCard = new DataUsageCard(m_themeService, this);
  connect(m_dataUsageCard,
          &DataUsageCard::clearRequested,
          this,
          &HomeView::dataUsageClearRequested);
  chartsRow->addWidget(m_dataUsageCard, 1);
  statsLayout->addLayout(chartsRow);
  mainLayout->addLayout(gridLayout);
  mainLayout->addStretch();
  // Signals
  if (m_proxyModeSection) {
    connect(m_proxyModeSection,
            &ProxyModeSection::systemProxyChanged,
            this,
            &HomeView::systemProxyChanged);
    connect(m_proxyModeSection,
            &ProxyModeSection::tunModeChanged,
            this,
            &HomeView::tunModeChanged);
    connect(m_proxyModeSection,
            &ProxyModeSection::proxyModeChanged,
            this,
            &HomeView::proxyModeChanged);
  }
}

void HomeView::updateStyle() {
  if (!m_themeService) {
    return;
  }
  QColor                 primary = m_themeService->color("primary");
  QColor                 success = m_themeService->color("success");
  QColor                 warning = m_themeService->color("warning");
  QColor                 error   = m_themeService->color("error");
  QMap<QString, QString> extra;
  // Determine correct trash icon based on text brightness
  // If text is dark (light theme), use dark icon. Otherwise use white icon.
  QColor  textPrimary  = m_themeService->color("text-primary");
  QString trashIconUrl = ":/icons/trash.svg";
  if (textPrimary.lightness() < 128) {
    trashIconUrl = ":/icons/trash-dark.svg";
  }
  extra.insert("trash-icon-url", trashIconUrl);
  extra.insert("success-12", rgba(success, 0.12));
  extra.insert("warning-12", rgba(warning, 0.12));
  extra.insert("error-12", rgba(error, 0.12));
  extra.insert("success-18", rgba(success, 0.18));
  extra.insert("primary-18", rgba(primary, 0.18));
  extra.insert("warning-18", rgba(warning, 0.18));
  extra.insert("primary-06", rgba(primary, 0.06));
  setStyleSheet(
      m_themeService->loadStyleSheet(":/styles/home_view.qss", extra));
  if (m_trafficChart) {
    m_trafficChart->updateStyle();
  }
  if (m_uploadCard) {
    m_uploadCard->updateStyle();
  }
  if (m_downloadCard) {
    m_downloadCard->updateStyle();
  }
  if (m_connectionsCard) {
    m_connectionsCard->updateStyle();
  }
  if (m_proxyModeSection) {
    m_proxyModeSection->updateStyle();
  }
  polishWidget(m_statusBadge);
  polishWidget(m_statusDot);
}

bool HomeView::isSystemProxyEnabled() const {
  return m_proxyModeSection && m_proxyModeSection->isSystemProxyEnabled();
}

void HomeView::setSystemProxyEnabled(bool enabled) {
  if (!m_proxyModeSection) {
    return;
  }
  m_proxyModeSection->setSystemProxyEnabled(enabled);
}

bool HomeView::isTunModeEnabled() const {
  return m_proxyModeSection && m_proxyModeSection->isTunModeEnabled();
}

void HomeView::setTunModeEnabled(bool enabled) {
  if (!m_proxyModeSection) {
    return;
  }
  m_proxyModeSection->setTunModeEnabled(enabled);
}

void HomeView::setProxyMode(const QString& mode) {
  if (!m_proxyModeSection) {
    return;
  }
  m_proxyModeSection->setProxyMode(mode);
}

void HomeView::updateStatus(bool running) {
  m_isRunning = running;
  m_statusText->setText(running ? tr("Running") : tr("Stopped"));
  m_statusBadge->setProperty("status", running ? "running" : "stopped");
  // Refresh style to update button color
  updateStyle();
  if (m_statusDot) {
    m_statusDot->setProperty("status", running ? "running" : "stopped");
    polishWidget(m_statusDot);
  }
  polishWidget(m_statusBadge);
  if (!running) {
    m_totalUpload   = 0;
    m_totalDownload = 0;
    m_trafficTimer.invalidate();
    if (m_trafficChart) {
      m_trafficChart->clear();
    }
    if (m_uploadCard) {
      m_uploadCard->setValueText(tr("0 B/s"));
    }
    if (m_downloadCard) {
      m_downloadCard->setValueText(tr("0 B/s"));
    }
    if (m_uploadCard) {
      m_uploadCard->setSubText(tr("Total: 0 B"));
    }
    if (m_downloadCard) {
      m_downloadCard->setSubText(tr("Total: 0 B"));
    }
  }
}

void HomeView::updateTraffic(qint64 upload, qint64 download) {
  if (m_uploadCard) {
    m_uploadCard->setValueText(formatBytes(upload) + "/s");
  }
  if (m_downloadCard) {
    m_downloadCard->setValueText(formatBytes(download) + "/s");
  }
  if (m_trafficChart) {
    m_trafficChart->updateData(upload, download);
  }
  if (!m_trafficTimer.isValid()) {
    m_trafficTimer.start();
  } else {
    const qint64 elapsedMs = m_trafficTimer.restart();
    const double seconds   = elapsedMs / 1000.0;
    if (seconds > 0) {
      m_totalUpload += static_cast<qint64>(upload * seconds);
      m_totalDownload += static_cast<qint64>(download * seconds);
    }
  }
  if (m_uploadCard) {
    m_uploadCard->setSubText(tr("Total: %1").arg(formatBytes(m_totalUpload)));
  }
  if (m_downloadCard) {
    m_downloadCard->setSubText(
        tr("Total: %1").arg(formatBytes(m_totalDownload)));
  }
}

void HomeView::updateUptime(int seconds) {
  if (!m_statusBadge) {
    return;
  }
  if (seconds <= 0) {
    m_statusBadge->setToolTip(QString());
    return;
  }
  m_statusBadge->setToolTip(tr("Uptime: %1").arg(formatDuration(seconds)));
}

void HomeView::updateConnections(int count, qint64 memoryUsage) {
  if (m_connectionsCard) {
    m_connectionsCard->setValueText(QString::number(count));
  }
  if (m_connectionsCard) {
    m_connectionsCard->setSubText(
        tr("Memory usage: %1").arg(formatBytes(memoryUsage)));
  }
}

void HomeView::updateDataUsage(const QJsonObject& snapshot) {
  if (m_dataUsageCard) {
    m_dataUsageCard->updateDataUsage(snapshot);
  }
}

QString HomeView::formatBytes(qint64 bytes) const {
  return HomeFormat::bytes(bytes);
}

QString HomeView::formatDuration(int seconds) const {
  return HomeFormat::duration(seconds);
}
