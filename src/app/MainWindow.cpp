#include "MainWindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QCloseEvent>
#include <QSettings>
#include <QApplication>
#include <QJsonArray>
#include <QJsonValue>
#include <QTimer>
#include <QFile>
#include <QMessageBox>
#include <QStyle>
#include <QPainter>
#include <QSvgRenderer>
#include "views/home/HomeView.h"
#include "views/proxy/ProxyView.h"
#include "views/subscription/SubscriptionView.h"
#include "views/connections/ConnectionsView.h"
#include "views/rules/RulesView.h"
#include "views/logs/LogView.h"
#include "views/settings/SettingsView.h"
#include "core/KernelService.h"
#include "core/ProxyService.h"
#include "core/ProxyController.h"
#include "utils/Logger.h"
#include "utils/ThemeManager.h"
#include "storage/DatabaseService.h"
#include "storage/AppSettings.h"
#include "services/config/ConfigManager.h"
#include "network/SubscriptionService.h"
#include "system/AdminHelper.h"

namespace {
QIcon svgIcon(const QString &resourcePath, int size, const QColor &color)
{
    const qreal dpr = qApp->devicePixelRatio();
    const int box = static_cast<int>(size * dpr);

    QSvgRenderer renderer(resourcePath);

    QPixmap base(box, box);
    base.fill(Qt::transparent);
    {
        QPainter p(&base);
        p.setRenderHint(QPainter::Antialiasing);
        QSizeF def = renderer.defaultSize();
        qreal w = def.width() > 0 ? def.width() : size;
        qreal h = def.height() > 0 ? def.height() : size;
        qreal ratio = (h > 0) ? w / h : 1.0;
        qreal renderW = box;
        qreal renderH = box;
        if (ratio > 1.0) {
            renderH = renderH / ratio;
        } else if (ratio < 1.0) {
            renderW = renderW * ratio;
        }
        QRectF target((box - renderW) / 2.0, (box - renderH) / 2.0, renderW, renderH);
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
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_kernelService(new KernelService(this)), m_proxyService(new ProxyService(this)) // Initialize ProxyService
      ,
      m_proxyController(new ProxyController(m_kernelService, nullptr, this))
{
    setupUI();
    m_proxyController->setSubscriptionService(m_subscriptionView ? m_subscriptionView->getService() : nullptr);
    setupConnections();
    loadSettings();
    if (m_homeView)
    {
        m_homeView->setSystemProxyEnabled(AppSettings::instance().systemProxyEnabled());
        m_homeView->setTunModeEnabled(false);
        QString configPath = m_proxyController->activeConfigPath();
        if (!configPath.isEmpty())
        {
            m_homeView->setProxyMode(ConfigManager::instance().readClashDefaultMode(configPath));
        }
    }
    updateStyle();
    Logger::info(QStringLiteral("Main window initialized"));
}

MainWindow::~MainWindow()
{
    saveSettings();
}

// ... existing code ...

void MainWindow::setupUI()
{
    setWindowTitle(tr("Sing-Box"));
    setMinimumSize(1000, 700);

    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout(m_centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QWidget *navContainer = new QWidget;
    navContainer->setObjectName("NavContainer");
    QVBoxLayout *navLayout = new QVBoxLayout(navContainer);
    navLayout->setContentsMargins(0, 20, 0, 20);
    navLayout->setSpacing(10);

    QLabel *logoLabel = new QLabel("Sing-Box");
    logoLabel->setObjectName("LogoLabel");
    logoLabel->setAlignment(Qt::AlignCenter);
    navLayout->addWidget(logoLabel);

    setupNavigation();
    navLayout->addWidget(m_navList, 1);

    QLabel *versionLabel = new QLabel("v" + QApplication::applicationVersion());
    versionLabel->setObjectName("VersionLabel");
    versionLabel->setAlignment(Qt::AlignCenter);
    navLayout->addWidget(versionLabel);

    mainLayout->addWidget(navContainer);

    QWidget *contentContainer = new QWidget;
    contentContainer->setObjectName("ContentContainer");
    QVBoxLayout *contentLayout = new QVBoxLayout(contentContainer);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_stackedWidget = new QStackedWidget;

    m_homeView = new HomeView;
    m_proxyView = new ProxyView;
    m_subscriptionView = new SubscriptionView;
    m_connectionsView = new ConnectionsView;
    m_rulesView = new RulesView;
    m_logView = new LogView;
    m_settingsView = new SettingsView;

    m_proxyView->setProxyService(m_proxyService);
    m_connectionsView->setProxyService(m_proxyService);
    m_rulesView->setProxyService(m_proxyService);

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

void MainWindow::setupNavigation()
{
    m_navList = new QListWidget;
    m_navList->setFixedWidth(200);
    m_navList->setIconSize(QSize(20, 20));
    m_navList->setFocusPolicy(Qt::NoFocus);

    struct NavItem
    {
        QString name;
        QString icon;
    };
    QList<NavItem> items = {
        {tr("Home"), "home"},
        {tr("Proxy"), "proxy"},
        {tr("Subscription"), "sub"},
        {tr("Connections"), "conn"},
        {tr("Rules"), "rule"},
        {tr("Logs"), "log"},
        {tr("Settings"), "settings"}};

    for (const auto &item : items)
    {
        QListWidgetItem *listItem = new QListWidgetItem(item.name);
        listItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_navList->addItem(listItem);
    }

    m_navList->setCurrentRow(0);

    updateNavIcons();
}

void MainWindow::setupStatusBar()
{
    QWidget *statusWidget = new QWidget;
    statusWidget->setObjectName("StatusBar");
    statusWidget->setFixedHeight(48);

    QHBoxLayout *statusLayout = new QHBoxLayout(statusWidget);
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

    QWidget *contentContainer = findChild<QWidget *>("ContentContainer");
    if (contentContainer)
    {
        contentContainer->layout()->addWidget(statusWidget);
    }
}

void MainWindow::setupConnections()
{
    setupNavigationConnections();
    setupKernelConnections();
    setupThemeConnections();
    setupSubscriptionConnections();
    setupProxyServiceConnections();
    setupHomeViewConnections();
    
    // Initialize states.
    m_homeView->updateStatus(m_kernelService->isRunning());
    m_connectionsView->setAutoRefreshEnabled(m_kernelService->isRunning());
}

void MainWindow::setupNavigationConnections()
{
    connect(m_navList, &QListWidget::itemClicked,
            this, &MainWindow::onNavigationItemClicked);

    connect(m_startStopBtn, &QPushButton::clicked,
            this, &MainWindow::onStartStopClicked);
}

void MainWindow::setupKernelConnections()
{
    connect(m_kernelService, &KernelService::statusChanged,
            this, &MainWindow::onKernelStatusChanged);

    connect(m_kernelService, &KernelService::statusChanged,
            m_homeView, &HomeView::updateStatus);

    connect(m_kernelService, &KernelService::outputReceived,
            m_logView, &LogView::appendLog);
    
    connect(m_kernelService, &KernelService::errorOccurred, this,
            [this](const QString &error)
            {
                m_logView->appendLog(QString("[ERROR] %1").arg(error));
            });
}

void MainWindow::setupThemeConnections()
{
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &MainWindow::updateStyle);
}

void MainWindow::setupSubscriptionConnections()
{
    connect(m_subscriptionView->getService(), &SubscriptionService::applyConfigRequested,
            this, [this](const QString &configPath, bool restart)
            {
                if (configPath.isEmpty()) return;
                m_kernelService->setConfigPath(configPath);
                if (restart && m_kernelService->isRunning()) {
                    m_kernelService->restartWithConfig(configPath);
                } });
}

void MainWindow::setupProxyServiceConnections()
{
    connect(m_proxyService, &ProxyService::trafficUpdated,
            m_homeView, &HomeView::updateTraffic);

    connect(m_proxyService, &ProxyService::connectionsReceived, this,
            [this](const QJsonObject &connections)
            {
                const QJsonArray conns = connections.value("connections").toArray();
                QJsonValue memoryValue = connections.value("memory");
                if (memoryValue.isUndefined())
                {
                    memoryValue = connections.value("memory_usage");
                }
                if (memoryValue.isUndefined())
                {
                    memoryValue = connections.value("memoryUsage");
                }
                const qint64 memoryUsage = memoryValue.toVariant().toLongLong();
                m_homeView->updateConnections(conns.count(), memoryUsage);
            });
}

void MainWindow::setupHomeViewConnections()
{
    connect(m_homeView, &HomeView::systemProxyChanged, this, [this](bool enabled)
            {
        if (m_proxyController) {
            m_proxyController->setSystemProxyEnabled(enabled);
        }
        if (enabled && m_homeView) {
            m_homeView->setTunModeEnabled(false);
        } });

    connect(m_homeView, &HomeView::tunModeChanged, this, [this](bool enabled)
            {
        if (enabled && !AdminHelper::isAdmin()) {
            QMessageBox box(this);
            box.setIcon(QMessageBox::Warning);
            box.setWindowTitle(tr("Administrator permission required"));
            box.setText(tr("Switching to TUN mode requires restarting with administrator privileges. Restart as administrator now?"));
            box.addButton(tr("Cancel"), QMessageBox::RejectRole);
            auto *restartBtn = box.addButton(tr("Restart as administrator"), QMessageBox::AcceptRole);
            box.setDefaultButton(restartBtn);
            box.exec();

            if (box.clickedButton() == restartBtn) {
                AppSettings::instance().setSystemProxyEnabled(false);
                AppSettings::instance().setTunEnabled(true);
                if (!AdminHelper::restartAsAdmin()) {
                    AppSettings::instance().setTunEnabled(false);
                    if (m_homeView) {
                        m_homeView->setTunModeEnabled(false);
                    }
                }
            } else {
                if (m_homeView) {
                    m_homeView->setTunModeEnabled(false);
                }
                AppSettings::instance().setTunEnabled(false);
            }
            return;
        }

        if (m_proxyController) {
            m_proxyController->setTunModeEnabled(enabled);
        }
        if (enabled && m_homeView) {
            m_homeView->setSystemProxyEnabled(false);
        } });

    connect(m_homeView, &HomeView::proxyModeChanged, this, [this](const QString &mode)
            {
        QString error;
        const bool restartKernel = m_kernelService && m_kernelService->isRunning();
        if (m_proxyController && m_proxyController->setProxyMode(mode, restartKernel, &error)) {
            const QString msg = QStringLiteral("Proxy mode switched to: %1").arg(mode);
            Logger::info(msg);
            if (m_logView) {
                m_logView->appendLog(QString("[INFO] %1").arg(msg));
            }
        } else {
            const QString msg = error.isEmpty() ? QStringLiteral("Failed to switch proxy mode") : error;
            Logger::error(msg);
            if (m_logView) {
                m_logView->appendLog(QString("[ERROR] %1").arg(msg));
            }
        } });
}


void MainWindow::onNavigationItemClicked(QListWidgetItem *item)
{
    int index = m_navList->row(item);
    m_stackedWidget->setCurrentIndex(index);
}

bool MainWindow::isKernelRunning() const
{
    return m_kernelService && m_kernelService->isRunning();
}

QString MainWindow::activeConfigPath() const
{
    return m_proxyController ? m_proxyController->activeConfigPath() : QString();
}

QString MainWindow::currentProxyMode() const
{
    return m_proxyController ? m_proxyController->currentProxyMode() : QString("rule");
}

void MainWindow::setProxyModeUI(const QString &mode)
{
    if (m_homeView)
    {
        m_homeView->setProxyMode(mode);
    }
}

void MainWindow::onKernelStatusChanged(bool running)
{
    m_connectionsView->setAutoRefreshEnabled(running);

    if (running)
    {
        m_startStopBtn->setText(tr("Stop"));

        m_startStopBtn->setProperty("state", "stop");
        applyStartStopStyle();

        m_proxyService->startTrafficMonitor();
        if (m_proxyController)
        {
            m_proxyController->updateSystemProxyForKernelState(true);
        }
        QTimer::singleShot(1000, m_proxyView, &ProxyView::refresh);
        QTimer::singleShot(1200, m_rulesView, &RulesView::refresh);
    }
    else
    {
        m_startStopBtn->setText(tr("Start"));
        m_startStopBtn->setProperty("state", "start");
        applyStartStopStyle();

        m_proxyService->stopTrafficMonitor();

        if (m_proxyController)
        {
            m_proxyController->updateSystemProxyForKernelState(false);
        }
    }
}

void MainWindow::onStartStopClicked()
{
    if (!m_proxyController || !m_proxyController->toggleKernel())
    {
        QMessageBox::warning(this, tr("Start kernel"), tr("Config file not found at the expected location; startup failed."));
    }
}

void MainWindow::updateStyle()
{
    ThemeManager &tm = ThemeManager::instance();
    setStyleSheet(tm.loadStyleSheet(":/styles/main_window.qss"));

    updateNavIcons();
    applyStartStopStyle();
}

void MainWindow::showAndActivate()
{
    show();
    raise();
    activateWindow();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}

void MainWindow::applyStartStopStyle()
{
    // Qt will automatically update styles when properties change.
    // We only need to polish if the style doesn't update automatically.
    m_startStopBtn->style()->polish(m_startStopBtn);
}

void MainWindow::updateNavIcons()
{
    if (!m_navList || m_navList->count() <= 1)
    {
        return;
    }

    ThemeManager &tm = ThemeManager::instance();
    const QColor iconColor = tm.getColor("text-primary");
    const int iconSize = 20;

    if (QListWidgetItem *homeItem = m_navList->item(0))
    {
        homeItem->setIcon(svgIcon(":/icons/house.svg", iconSize, iconColor));
    }
    if (QListWidgetItem *proxyItem = m_navList->item(1))
    {
        proxyItem->setIcon(svgIcon(":/icons/network.svg", iconSize, iconColor));
    }
    if (QListWidgetItem *subItem = m_navList->item(2))
    {
        subItem->setIcon(svgIcon(":/icons/subscriptions.svg", iconSize, iconColor));
    }
    if (QListWidgetItem *connItem = m_navList->item(3))
    {
        connItem->setIcon(svgIcon(":/icons/connections.svg", iconSize, iconColor));
    }
    if (QListWidgetItem *rulesItem = m_navList->item(4))
    {
        rulesItem->setIcon(svgIcon(":/icons/checklist.svg", iconSize, iconColor));
    }
    if (QListWidgetItem *logsItem = m_navList->item(5))
    {
        logsItem->setIcon(svgIcon(":/icons/logs.svg", iconSize, iconColor));
    }
    if (QListWidgetItem *settingsItem = m_navList->item(6))
    {
        settingsItem->setIcon(svgIcon(":/icons/slider.svg", iconSize, iconColor));
    }
    m_navList->setIconSize(QSize(iconSize, iconSize));
}


void MainWindow::loadSettings()
{
    QSettings settings;
    restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
    restoreState(settings.value("mainWindow/state").toByteArray());
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("mainWindow/geometry", saveGeometry());
    settings.setValue("mainWindow/state", saveState());
}
