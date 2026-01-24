#include "MainWindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QCloseEvent>
#include <QSettings>
#include <QSplitter>
#include <QCloseEvent>
#include <QSettings>
#include <QApplication>
#include <QJsonArray>
#include <QJsonValue>
#include <QTimer>
#include <QFile>
#include <QMessageBox>
#include "views/HomeView.h"
#include "views/ProxyView.h"
#include "views/SubscriptionView.h"
#include "views/ConnectionsView.h"
#include "views/RulesView.h"
#include "views/LogView.h"
#include "views/SettingsView.h"
#include "core/KernelService.h"
#include "core/ProxyService.h"
#include "core/ProxyController.h"
#include "utils/Logger.h"
#include "utils/ThemeManager.h"
#include "storage/DatabaseService.h"
#include "storage/AppSettings.h"
#include "services/ConfigManager.h"
#include "network/SubscriptionService.h"
#include "system/AdminHelper.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_kernelService(new KernelService(this))
    , m_proxyService(new ProxyService(this)) // Initialize ProxyService
    , m_proxyController(new ProxyController(m_kernelService, nullptr, this))
{
    setupUI();
    m_proxyController->setSubscriptionService(m_subscriptionView ? m_subscriptionView->getService() : nullptr);
    setupConnections();
    loadSettings();
    if (m_homeView) {
        m_homeView->setSystemProxyEnabled(AppSettings::instance().systemProxyEnabled());
        m_homeView->setTunModeEnabled(false);
        QString configPath = m_proxyController->activeConfigPath();
        if (!configPath.isEmpty()) {
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
    logoLabel->setStyleSheet("font-size: 20px; font-weight: bold; padding: 10px;");
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


    struct NavItem { QString name; QString icon; };
    QList<NavItem> items = {
        {tr("Home"), "home"},
        {tr("Proxy"), "proxy"},
        {tr("Subscription"), "sub"},
        {tr("Connections"), "conn"},
        {tr("Rules"), "rule"},
        {tr("Logs"), "log"},
        {tr("Settings"), "settings"}
    };

    for (const auto &item : items) {
        QListWidgetItem *listItem = new QListWidgetItem(item.name);
        listItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_navList->addItem(listItem);
    }

    m_navList->setCurrentRow(0);
}

void MainWindow::setupStatusBar()
{
    QWidget *statusWidget = new QWidget;
    statusWidget->setObjectName("StatusBar");
    statusWidget->setFixedHeight(48);

    QHBoxLayout *statusLayout = new QHBoxLayout(statusWidget);
    statusLayout->setContentsMargins(20, 0, 20, 0);



    m_startStopBtn = new QPushButton(tr("Start"));
    m_startStopBtn->setFixedSize(80, 32);
    m_startStopBtn->setCursor(Qt::PointingHandCursor);
    
    // Initial Transparent Green
    QString startStyle = QString(
        "QPushButton { background-color: rgba(16, 185, 129, 0.2); color: #10b981; border: 1px solid rgba(16, 185, 129, 0.4); "
        "border-radius: 10px; font-weight: bold; }"
        "QPushButton:hover { background-color: rgba(16, 185, 129, 0.3); }"
    );
    m_startStopBtn->setStyleSheet(startStyle);

    statusLayout->addStretch();
    statusLayout->addWidget(m_startStopBtn);


    QWidget *contentContainer = findChild<QWidget*>("ContentContainer");
    if (contentContainer) {
        contentContainer->layout()->addWidget(statusWidget);
    }
}

void MainWindow::setupConnections()
{
    connect(m_navList, &QListWidget::itemClicked, 
            this, &MainWindow::onNavigationItemClicked);

    connect(m_startStopBtn, &QPushButton::clicked,
            this, &MainWindow::onStartStopClicked);

    connect(m_kernelService, &KernelService::statusChanged,
            this, &MainWindow::onKernelStatusChanged);

    connect(m_kernelService, &KernelService::statusChanged,
            m_homeView, &HomeView::updateStatus);

    connect(m_kernelService, &KernelService::outputReceived,
            m_logView, &LogView::appendLog);
    connect(m_kernelService, &KernelService::errorOccurred, this,
            [this](const QString &error) {
                m_logView->appendLog(QString("[ERROR] %1").arg(error));
            });


    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &MainWindow::updateStyle);


    connect(m_subscriptionView->getService(), &SubscriptionService::applyConfigRequested,
            this, [this](const QString &configPath, bool restart) {
                if (configPath.isEmpty()) return;
                m_kernelService->setConfigPath(configPath);
                if (restart && m_kernelService->isRunning()) {
                    m_kernelService->restartWithConfig(configPath);
                }
            });


    connect(m_proxyService, &ProxyService::trafficUpdated,
            m_homeView, &HomeView::updateTraffic);



    connect(m_proxyService, &ProxyService::connectionsReceived, this,
            [this](const QJsonObject &connections) {
                const QJsonArray conns = connections.value("connections").toArray();
                QJsonValue memoryValue = connections.value("memory");
                if (memoryValue.isUndefined()) {
                    memoryValue = connections.value("memory_usage");
                }
                if (memoryValue.isUndefined()) {
                    memoryValue = connections.value("memoryUsage");
                }
                const qint64 memoryUsage = memoryValue.toVariant().toLongLong();
                m_homeView->updateConnections(conns.count(), memoryUsage);
            });

    connect(m_homeView, &HomeView::systemProxyChanged, this, [this](bool enabled) {
        bool ok = true;
        if (m_proxyController) {
            ok = m_proxyController->setSystemProxyEnabled(enabled);
        }
        if (!ok) {
            if (m_homeView) {
                m_homeView->setSystemProxyEnabled(false);
            }
            QMessageBox::warning(this, tr("System Proxy"), tr("Failed to update system proxy settings."));
            return;
        }
        if (enabled && m_homeView) {
            m_homeView->setTunModeEnabled(false);
        }
    });

    connect(m_homeView, &HomeView::tunModeChanged, this, [this](bool enabled) {
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
        }
    });

    connect(m_homeView, &HomeView::proxyModeChanged, this, [this](const QString &mode) {
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
        }
    });

    m_homeView->updateStatus(m_kernelService->isRunning());
    m_connectionsView->setAutoRefreshEnabled(m_kernelService->isRunning());
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
    if (m_homeView) {
        m_homeView->setProxyMode(mode);
    }
}

void MainWindow::onKernelStatusChanged(bool running)
{
    ThemeManager &tm = ThemeManager::instance();
    m_connectionsView->setAutoRefreshEnabled(running);

    if (running) {
        m_startStopBtn->setText(tr("Stop"));
        
        // Transparent Red for Stop
        QString style = QString(
            "QPushButton { background-color: rgba(233, 69, 96, 0.2); color: #e94560; border: 1px solid rgba(233, 69, 96, 0.4); "
            "border-radius: 10px; font-weight: bold; }"
            "QPushButton:hover { background-color: rgba(233, 69, 96, 0.3); }"
        );
        m_startStopBtn->setStyleSheet(style);

        m_proxyService->startTrafficMonitor();
        if (m_proxyController) {
            m_proxyController->updateSystemProxyForKernelState(true);
        }
        QTimer::singleShot(1000, m_proxyView, &ProxyView::refresh);
        QTimer::singleShot(1200, m_rulesView, &RulesView::refresh);

    } else {
        m_startStopBtn->setText(tr("Start"));
        
        ThemeManager &tm = ThemeManager::instance();
        // Transparent Green for Start
        QString style = QString(
            "QPushButton { background-color: rgba(16, 185, 129, 0.2); color: #10b981; border: 1px solid rgba(16, 185, 129, 0.4); "
            "border-radius: 10px; font-weight: bold; }"
            "QPushButton:hover { background-color: rgba(16, 185, 129, 0.3); }"
        );
        m_startStopBtn->setStyleSheet(style);

        m_proxyService->stopTrafficMonitor();

        if (m_proxyController) {
            m_proxyController->updateSystemProxyForKernelState(false);
        }
    }
}

void MainWindow::onStartStopClicked()
{
    if (!m_proxyController || !m_proxyController->toggleKernel()) {
        QMessageBox::warning(this, tr("Start kernel"), tr("Config file not found at the expected location; startup failed."));
    }
}

void MainWindow::updateStyle()
{
    ThemeManager &tm = ThemeManager::instance();
    setStyleSheet(tm.loadStyleSheet(":/styles/main_window.qss"));

    if (m_startStopBtn) {
       // Only apply button style if not customized (or rely on onKernelStatusChanged to re-apply)
       // Actually, onKernelStatusChanged handles dynamic colors. Initial setup handles initial color.
       // We might lose state if theme changes, but onKernelStatusChanged should be called or we can re-apply based on valid status.
       // For now, let's NOT override it with generic button style.
    }
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
