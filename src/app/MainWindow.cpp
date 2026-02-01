#include "MainWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPainter>
#include <QSettings>
#include <QSplitter>
#include <QStyle>
#include <QSvgRenderer>
#include <QVBoxLayout>

#include "app/AppContext.h"
#include "app/ProxyRuntimeController.h"
#include "app/ProxyUiController.h"
#include "app/interfaces/AdminActions.h"
#include "app/interfaces/SettingsStore.h"
#include "app/interfaces/ThemeService.h"
#include "core/KernelService.h"
#include "core/ProxyController.h"
#include "network/SubscriptionService.h"
#include "services/config/ConfigManager.h"
#include "storage/AppSettings.h"
#include "storage/DatabaseService.h"
#include "system/AdminHelper.h"
#include "utils/Logger.h"
#include "utils/ThemeManager.h"
#include "views/connections/ConnectionsView.h"
#include "views/home/HomeView.h"
#include "views/logs/LogView.h"
#include "views/proxy/ProxyView.h"
#include "views/rules/RulesView.h"
#include "views/settings/SettingsView.h"
#include "views/subscription/SubscriptionView.h"
namespace {
QIcon svgIcon(const QString& resourcePath, int size, const QColor& color) {
  const qreal dpr = qApp->devicePixelRatio();
  const int   box = static_cast<int>(size * dpr);

  QSvgRenderer renderer(resourcePath);

  QPixmap base(box, box);
  base.fill(Qt::transparent);
  {
    QPainter p(&base);
    p.setRenderHint(QPainter::Antialiasing);
    QSizeF def     = renderer.defaultSize();
    qreal  w       = def.width() > 0 ? def.width() : size;
    qreal  h       = def.height() > 0 ? def.height() : size;
    qreal  ratio   = (h > 0) ? w / h : 1.0;
    qreal  renderW = box;
    qreal  renderH = box;
    if (ratio > 1.0) {
      renderH = renderH / ratio;
    } else if (ratio < 1.0) {
      renderW = renderW * ratio;
    }
    QRectF target((box - renderW) / 2.0, (box - renderH) / 2.0, renderW,
                  renderH);
    renderer.render(&p, target);
  }

  QPixmap tinted(box, box);
  tinted.fill(Qt::transparent);
  {
    QPainter p(&tinted);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawPixmap(0, 0, base);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(tinted.rect(), color);
  }

  tinted.setDevicePixelRatio(dpr);
  return QIcon(tinted);
}
}  // namespace
MainWindow::MainWindow(AppContext& ctx, QWidget* parent)
    : QMainWindow(parent),
      m_ctx(ctx),
      m_proxyController(ctx.proxyController()),
      m_proxyUiController(ctx.proxyUiController()),
      m_proxyRuntimeController(ctx.proxyRuntimeController()),
      m_subscriptionService(ctx.subscriptionService()),
      m_settingsStore(ctx.settingsStore()),
      m_themeService(ctx.themeService()),
      m_adminActions(ctx.adminActions()) {
  setupUI();
  if (m_proxyController) {
    m_proxyController->setSubscriptionService(m_subscriptionService);
  }
  setupConnections();
  loadSettings();
  if (m_homeView) {
    const bool sysProxy =
        m_proxyUiController
            ? m_proxyUiController->systemProxyEnabled()
            : (m_settingsStore ? m_settingsStore->systemProxyEnabled() : false);
    const bool tunEnabled =
        m_proxyUiController ? m_proxyUiController->tunModeEnabled() : false;
    m_homeView->setSystemProxyEnabled(sysProxy);
    m_homeView->setTunModeEnabled(tunEnabled);
    m_homeView->setProxyMode(m_proxyUiController
                                 ? m_proxyUiController->currentProxyMode()
                                 : QStringLiteral("rule"));
  }
  updateStyle();
  Logger::info(QStringLiteral("Main window initialized"));
}
MainWindow::~MainWindow() { saveSettings(); }
// ... existing code ...

void MainWindow::setupUI() {
  setWindowTitle(tr("Sing-Box"));
  setMinimumSize(1000, 700);

  m_centralWidget = new QWidget(this);
  setCentralWidget(m_centralWidget);

  QHBoxLayout* mainLayout = new QHBoxLayout(m_centralWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  QWidget* navContainer = new QWidget;
  navContainer->setObjectName("NavContainer");
  QVBoxLayout* navLayout = new QVBoxLayout(navContainer);
  navLayout->setContentsMargins(0, 20, 0, 20);
  navLayout->setSpacing(10);

  QLabel* logoLabel = new QLabel("Sing-Box");
  logoLabel->setObjectName("LogoLabel");
  logoLabel->setAlignment(Qt::AlignCenter);
  navLayout->addWidget(logoLabel);

  setupNavigation();
  navLayout->addWidget(m_navList, 1);

  QLabel* versionLabel = new QLabel("v" + QApplication::applicationVersion());
  versionLabel->setObjectName("VersionLabel");
  versionLabel->setAlignment(Qt::AlignCenter);
  navLayout->addWidget(versionLabel);

  mainLayout->addWidget(navContainer);

  QWidget* contentContainer = new QWidget;
  contentContainer->setObjectName("ContentContainer");
  QVBoxLayout* contentLayout = new QVBoxLayout(contentContainer);
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(0);

  m_stackedWidget = new QStackedWidget;

  m_homeView  = new HomeView(m_themeService);
  m_proxyView = new ProxyView(m_themeService, this);
  m_subscriptionView =
      new SubscriptionView(m_subscriptionService, m_themeService, this);
  m_connectionsView = new ConnectionsView(m_themeService);
  m_rulesView       = new RulesView(m_ctx.configRepository(), m_themeService);
  m_logView         = new LogView(m_themeService);
  m_settingsView    = new SettingsView(m_themeService);

  m_proxyView->setController(m_ctx.proxyViewController());
  if (m_ctx.proxyService()) {
    m_connectionsView->setProxyService(m_ctx.proxyService());
    m_rulesView->setProxyService(m_ctx.proxyService());
  }

  m_stackedWidget->addWidget(m_homeView);
  m_stackedWidget->addWidget(m_proxyView);
  m_stackedWidget->addWidget(m_subscriptionView);
  m_stackedWidget->addWidget(m_connectionsView);
  m_stackedWidget->addWidget(m_rulesView);
  m_stackedWidget->addWidget(m_logView);
  m_stackedWidget->addWidget(m_settingsView);

  contentLayout->addWidget(m_stackedWidget, 1);

  mainLayout->addWidget(contentContainer, 1);

  setupStatusBar();
}
void MainWindow::setupNavigation() {
  m_navList = new QListWidget;
  m_navList->setFixedWidth(200);
  m_navList->setIconSize(QSize(20, 20));
  m_navList->setFocusPolicy(Qt::NoFocus);
  struct NavItem {
    QString name;
    QString icon;
  };
  QList<NavItem> items = {
      {tr("Home"), "home"},        {tr("Proxy"), "proxy"},
      {tr("Subscription"), "sub"}, {tr("Connections"), "conn"},
      {tr("Rules"), "rule"},       {tr("Logs"), "log"},
      {tr("Settings"), "settings"}};

  for (const auto& item : items) {
    QListWidgetItem* listItem = new QListWidgetItem(item.name);
    listItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_navList->addItem(listItem);
  }

  m_navList->setCurrentRow(0);

  updateNavIcons();
}
void MainWindow::setupStatusBar() {
  QWidget* statusWidget = new QWidget;
  statusWidget->setObjectName("StatusBar");
  statusWidget->setFixedHeight(48);

  QHBoxLayout* statusLayout = new QHBoxLayout(statusWidget);
  statusLayout->setContentsMargins(20, 0, 20, 0);

  m_startStopBtn = new QPushButton(tr("Start"));
  m_startStopBtn->setFixedHeight(36);
  m_startStopBtn->setMinimumWidth(88);
  m_startStopBtn->setCursor(Qt::PointingHandCursor);
  m_startStopBtn->setObjectName("StartStopBtn");
  m_startStopBtn->setProperty("state", "start");
  applyStartStopStyle();

  statusLayout->addStretch();
  statusLayout->addWidget(m_startStopBtn);

  QWidget* contentContainer = findChild<QWidget*>("ContentContainer");
  if (contentContainer) {
    contentContainer->layout()->addWidget(statusWidget);
  }
}
void MainWindow::setupConnections() {
  setupNavigationConnections();
  setupKernelConnections();
  setupThemeConnections();
  setupSubscriptionConnections();
  setupProxyServiceConnections();
  setupHomeViewConnections();
  setupProxyUiBindings();
  setupRuntimeConnections();
  if (m_proxyUiController) {
    m_proxyUiController->broadcastCurrentStates();
  }
  if (m_proxyRuntimeController) {
    m_proxyRuntimeController->broadcastStates();
  } else {
    if (m_homeView) m_homeView->updateStatus(false);
    if (m_connectionsView) m_connectionsView->setAutoRefreshEnabled(false);
  }
}
void MainWindow::setupNavigationConnections() {
  connect(m_navList, &QListWidget::itemClicked, this,
          &MainWindow::onNavigationItemClicked);

  connect(m_startStopBtn, &QPushButton::clicked, this,
          &MainWindow::onStartStopClicked);
}
void MainWindow::setupKernelConnections() {
  if (!m_proxyRuntimeController) return;

  connect(m_proxyRuntimeController,
          &ProxyRuntimeController::kernelRunningChanged, this,
          &MainWindow::onKernelStatusChanged);

  if (m_homeView) {
    connect(m_proxyRuntimeController,
            &ProxyRuntimeController::kernelRunningChanged, m_homeView,
            &HomeView::updateStatus);
  }
  if (m_connectionsView) {
    connect(m_proxyRuntimeController,
            &ProxyRuntimeController::kernelRunningChanged, m_connectionsView,
            &ConnectionsView::setAutoRefreshEnabled);
  }
}
void MainWindow::setupThemeConnections() {
  if (m_themeService) {
    connect(m_themeService, &ThemeService::themeChanged, this,
            &MainWindow::updateStyle);
  }
}
void MainWindow::setupSubscriptionConnections() {
  if (!m_subscriptionService) return;

  connect(m_subscriptionService, &SubscriptionService::applyConfigRequested,
          this, [this](const QString& configPath, bool restart) {
            if (configPath.isEmpty()) return;
            if (!m_proxyController) return;
            if (restart) {
              m_proxyController->restartKernelWithConfig(configPath);
            } else if (auto* kernel = m_proxyController->kernel()) {
              kernel->setConfigPath(configPath);
            }
          });
}
void MainWindow::setupProxyServiceConnections() {
  if (!m_proxyRuntimeController) return;

  if (m_homeView) {
    connect(m_proxyRuntimeController, &ProxyRuntimeController::trafficUpdated,
            m_homeView, &HomeView::updateTraffic);
    connect(m_proxyRuntimeController,
            &ProxyRuntimeController::connectionsUpdated, this,
            [this](int count, qint64 memoryUsage) {
              if (m_homeView) {
                m_homeView->updateConnections(count, memoryUsage);
              }
            });
  }
}
void MainWindow::setupProxyUiBindings() {
  if (!m_proxyUiController || !m_homeView) return;

  connect(m_proxyUiController, &ProxyUiController::systemProxyStateChanged,
          m_homeView, &HomeView::setSystemProxyEnabled);
  connect(m_proxyUiController, &ProxyUiController::tunModeStateChanged,
          m_homeView, &HomeView::setTunModeEnabled);
  connect(m_proxyUiController, &ProxyUiController::proxyModeChanged, m_homeView,
          &HomeView::setProxyMode);
}
void MainWindow::setupRuntimeConnections() {
  if (!m_proxyRuntimeController) return;

  if (m_logView) {
    connect(m_proxyRuntimeController, &ProxyRuntimeController::logMessage,
            m_logView,
            [this](const QString& msg, bool) { m_logView->appendLog(msg); });
  }
  if (m_proxyView) {
    connect(m_proxyRuntimeController,
            &ProxyRuntimeController::refreshProxyViewRequested, m_proxyView,
            &ProxyView::refresh);
  }
  if (m_rulesView) {
    connect(m_proxyRuntimeController,
            &ProxyRuntimeController::refreshRulesViewRequested, m_rulesView,
            &RulesView::refresh);
  }
}
void MainWindow::setupHomeViewConnections() {
  if (!m_homeView) return;

  connect(
      m_homeView, &HomeView::systemProxyChanged, this, [this](bool enabled) {
        if (!m_proxyUiController) return;
        QString error;
        if (!m_proxyUiController->setSystemProxyEnabled(enabled, &error)) {
          if (!error.isEmpty()) {
            QMessageBox::warning(this, tr("System Proxy"), error);
          }
          m_homeView->setSystemProxyEnabled(
              m_proxyUiController->systemProxyEnabled());
          m_homeView->setTunModeEnabled(m_proxyUiController->tunModeEnabled());
        }
      });

  connect(m_homeView, &HomeView::tunModeChanged, this, [this](bool enabled) {
    if (!m_proxyUiController) return;
    const auto result =
        m_proxyUiController->setTunModeEnabled(enabled, [this]() {
          QMessageBox box(this);
          box.setIcon(QMessageBox::Warning);
          box.setWindowTitle(tr("Administrator permission required"));
          box.setText(
              tr("Switching to TUN mode requires restarting with administrator "
                 "privileges. Restart as administrator now?"));
          box.addButton(tr("Cancel"), QMessageBox::RejectRole);
          auto* restartBtn = box.addButton(tr("Restart as administrator"),
                                           QMessageBox::AcceptRole);
          box.setDefaultButton(restartBtn);
          box.exec();
          return box.clickedButton() == restartBtn;
        });

    if (result == ProxyUiController::TunResult::Failed ||
        result == ProxyUiController::TunResult::Cancelled) {
      m_homeView->setTunModeEnabled(m_proxyUiController->tunModeEnabled());
      m_homeView->setSystemProxyEnabled(
          m_proxyUiController->systemProxyEnabled());
    }
  });

  connect(m_homeView, &HomeView::proxyModeChanged, this,
          [this](const QString& mode) {
            if (!m_proxyUiController) return;

            QString error;
            if (m_proxyUiController->switchProxyMode(mode, &error)) {
              const QString msg =
                  QStringLiteral("Proxy mode switched to: %1").arg(mode);
              Logger::info(msg);
              if (m_logView) {
                m_logView->appendLog(QString("[INFO] %1").arg(msg));
              }
            } else {
              const QString msg =
                  error.isEmpty()
                      ? QStringLiteral("Failed to switch proxy mode")
                      : error;
              Logger::error(msg);
              if (m_logView) {
                m_logView->appendLog(QString("[ERROR] %1").arg(msg));
              }
              QMessageBox::warning(this, tr("Switch Mode Failed"), msg);
              m_homeView->setProxyMode(m_proxyUiController->currentProxyMode());
            }
          });
}
void MainWindow::onNavigationItemClicked(QListWidgetItem* item) {
  int index = m_navList->row(item);
  m_stackedWidget->setCurrentIndex(index);
}
bool MainWindow::isKernelRunning() const {
  if (m_proxyUiController) {
    return m_proxyUiController->isKernelRunning();
  }
  return m_proxyRuntimeController &&
         m_proxyRuntimeController->isKernelRunning();
}
QString MainWindow::activeConfigPath() const {
  return m_proxyController ? m_proxyController->activeConfigPath() : QString();
}
QString MainWindow::currentProxyMode() const {
  return m_proxyUiController ? m_proxyUiController->currentProxyMode()
                             : QStringLiteral("rule");
}
void MainWindow::setProxyModeUI(const QString& mode) {
  if (m_homeView) {
    m_homeView->setProxyMode(mode);
  }
}
void MainWindow::onKernelStatusChanged(bool running) {
  if (!m_startStopBtn) return;
  m_startStopBtn->setText(running ? tr("Stop") : tr("Start"));
  m_startStopBtn->setProperty("state", running ? "stop" : "start");
  applyStartStopStyle();
}
void MainWindow::onStartStopClicked() {
  QString error;
  if (!m_proxyUiController || !m_proxyUiController->toggleKernel(&error)) {
    if (error.isEmpty()) {
      error =
          tr("Config file not found at the expected location; startup failed.");
    }
    QMessageBox::warning(this, tr("Start kernel"), error);
  }
}
void MainWindow::updateStyle() {
  if (m_themeService) {
    setStyleSheet(m_themeService->loadStyleSheet(":/styles/main_window.qss"));
  }

  updateNavIcons();
  applyStartStopStyle();
}
void MainWindow::showAndActivate() {
  show();
  raise();
  activateWindow();
}
void MainWindow::closeEvent(QCloseEvent* event) {
  hide();
  event->ignore();
}
void MainWindow::applyStartStopStyle() {
  // Qt will automatically update styles when properties change.
  // We only need to polish if the style doesn't update automatically.
  m_startStopBtn->style()->polish(m_startStopBtn);
}
void MainWindow::updateNavIcons() {
  if (!m_navList || m_navList->count() <= 1) {
    return;
  }

  QColor iconColor;
  if (m_themeService) {
    iconColor = m_themeService->color("text-primary");
  }
  const int iconSize = 20;

  if (QListWidgetItem* homeItem = m_navList->item(0)) {
    homeItem->setIcon(svgIcon(":/icons/house.svg", iconSize, iconColor));
  }
  if (QListWidgetItem* proxyItem = m_navList->item(1)) {
    proxyItem->setIcon(svgIcon(":/icons/network.svg", iconSize, iconColor));
  }
  if (QListWidgetItem* subItem = m_navList->item(2)) {
    subItem->setIcon(svgIcon(":/icons/subscriptions.svg", iconSize, iconColor));
  }
  if (QListWidgetItem* connItem = m_navList->item(3)) {
    connItem->setIcon(svgIcon(":/icons/connections.svg", iconSize, iconColor));
  }
  if (QListWidgetItem* rulesItem = m_navList->item(4)) {
    rulesItem->setIcon(svgIcon(":/icons/checklist.svg", iconSize, iconColor));
  }
  if (QListWidgetItem* logsItem = m_navList->item(5)) {
    logsItem->setIcon(svgIcon(":/icons/logs.svg", iconSize, iconColor));
  }
  if (QListWidgetItem* settingsItem = m_navList->item(6)) {
    settingsItem->setIcon(svgIcon(":/icons/slider.svg", iconSize, iconColor));
  }
  m_navList->setIconSize(QSize(iconSize, iconSize));
}
void MainWindow::loadSettings() {
  QSettings settings;
  restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
  restoreState(settings.value("mainWindow/state").toByteArray());
}
void MainWindow::saveSettings() {
  QSettings settings;
  settings.setValue("mainWindow/geometry", saveGeometry());
  settings.setValue("mainWindow/state", saveState());
}
