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
#include "utils/Logger.h"
#include "utils/ThemeManager.h"
#include "storage/DatabaseService.h"
#include "storage/AppSettings.h"
#include "storage/ConfigManager.h"
#include "network/SubscriptionService.h"
#include "system/AdminHelper.h"
#include "system/SystemProxy.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_kernelService(new KernelService(this))
    , m_proxyService(new ProxyService(this)) // Initialize ProxyService
{
    setupUI();
    setupConnections();
    loadSettings();
    if (m_homeView) {
        m_homeView->setSystemProxyEnabled(AppSettings::instance().systemProxyEnabled());
        m_homeView->setTunModeEnabled(false);
        QString configPath;
        if (m_subscriptionView && m_subscriptionView->getService()) {
            configPath = m_subscriptionView->getService()->getActiveConfigPath();
        }
        if (configPath.isEmpty()) {
            configPath = ConfigManager::instance().getActiveConfigPath();
        }
        if (!configPath.isEmpty()) {
            m_homeView->setProxyMode(ConfigManager::instance().readClashDefaultMode(configPath));
        }
    }
    updateStyle(); // 搴旂敤鍒濆鏍峰紡
    
    Logger::info(QStringLiteral(u"\u4e3b\u7a97\u53e3\u521d\u59cb\u5316\u5b8c\u6210"));
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
    
    // 涓ぎ閮ㄤ欢
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);
    
    // 涓诲竷灞�
    QHBoxLayout *mainLayout = new QHBoxLayout(m_centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // 宸︿晶瀵艰埅鏍忓鍣?
    QWidget *navContainer = new QWidget;
    navContainer->setObjectName("NavContainer");
    QVBoxLayout *navLayout = new QVBoxLayout(navContainer);
    navLayout->setContentsMargins(0, 20, 0, 20);
    navLayout->setSpacing(10);
    
    // 椤堕儴 Logo 鍖哄煙 (TODO: 娣诲姞 Logo 鍥剧墖)
    QLabel *logoLabel = new QLabel("Sing-Box");
    logoLabel->setObjectName("LogoLabel");
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setStyleSheet("font-size: 20px; font-weight: bold; padding: 10px;");
    navLayout->addWidget(logoLabel);

    setupNavigation();
    navLayout->addWidget(m_navList, 1);
    
    // 搴曢儴鐗堟湰淇℃伅
    QLabel *versionLabel = new QLabel("v" + QApplication::applicationVersion());
    versionLabel->setObjectName("VersionLabel");
    versionLabel->setAlignment(Qt::AlignCenter);
    navLayout->addWidget(versionLabel);
    
    mainLayout->addWidget(navContainer);
    
    // 鍙充晶鍐呭鍖哄鍣?
    QWidget *contentContainer = new QWidget;
    contentContainer->setObjectName("ContentContainer");
    QVBoxLayout *contentLayout = new QVBoxLayout(contentContainer);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    
    // 椤甸潰鏍?
    m_stackedWidget = new QStackedWidget;
    
    // 鍒涘缓瑙嗗浘
    m_homeView = new HomeView;
    m_proxyView = new ProxyView;
    m_subscriptionView = new SubscriptionView;
    m_connectionsView = new ConnectionsView;
    m_rulesView = new RulesView;
    m_logView = new LogView;
    m_settingsView = new SettingsView;
    
    // 娉ㄥ叆 ProxyService 渚濊禆
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
    
    // 鎶?contentContainer 鍔犲叆涓诲竷灞�锛侊紒锛?
    mainLayout->addWidget(contentContainer, 1);
    
    // 鐘舵�佹爮
    setupStatusBar();
}

void MainWindow::setupNavigation()
{
    m_navList = new QListWidget;
    m_navList->setFixedWidth(200);
    m_navList->setIconSize(QSize(20, 20));
    m_navList->setFocusPolicy(Qt::NoFocus);
    
    // 瀵艰埅椤?
    struct NavItem { QString name; QString icon; };
    QList<NavItem> items = {
        {tr("\u9996\u9875"), "home"},
        {tr("\u4ee3\u7406"), "proxy"},
        {tr("\u8ba2\u9605"), "sub"},
        {tr("\u8fde\u63a5"), "conn"},
        {tr("\u89c4\u5219"), "rule"},
        {tr("\u65e5\u5fd7"), "log"},
        {tr("\u8bbe\u7f6e"), "settings"}
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
    
    
    
    m_startStopBtn = new QPushButton(tr("\u542f\u52a8"));
    m_startStopBtn->setFixedSize(80, 32);
    m_startStopBtn->setCursor(Qt::PointingHandCursor);
    
    statusLayout->addStretch();
    statusLayout->addWidget(m_startStopBtn);
    
    // 娣诲姞鍒板唴瀹瑰尯搴曢儴
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
            
    // 杩炴帴涓婚鍙樻洿淇″彿
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &MainWindow::updateStyle);
            
    // 璁㈤槄閰嶇疆搴旂敤璇锋眰锛堝垏鎹?鍒锋柊/缂栬緫閰嶇疆锛?
    connect(m_subscriptionView->getService(), &SubscriptionService::applyConfigRequested,
            this, [this](const QString &configPath, bool restart) {
                if (configPath.isEmpty()) return;
                m_kernelService->setConfigPath(configPath);
                if (restart && m_kernelService->isRunning()) {
                    m_kernelService->restartWithConfig(configPath);
                }
            });
            
    // 杩炴帴娴侀噺缁熻
    connect(m_proxyService, &ProxyService::trafficUpdated,
            m_homeView, &HomeView::updateTraffic);
            
            
    // 杩炴帴绯荤粺浠ｇ悊寮�鍏?
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
        if (enabled) {
            int port = ConfigManager::instance().getMixedPort();
            SystemProxy::setProxy("127.0.0.1", port);
        } else {
            SystemProxy::clearProxy();
        }
        AppSettings::instance().setSystemProxyEnabled(enabled);
        if (enabled) {
            AppSettings::instance().setTunEnabled(false);
            if (m_homeView) {
                m_homeView->setTunModeEnabled(false);
            }
        }

        QString configPath;
        if (m_subscriptionView && m_subscriptionView->getService()) {
            configPath = m_subscriptionView->getService()->getActiveConfigPath();
        }
        if (configPath.isEmpty()) {
            configPath = ConfigManager::instance().getActiveConfigPath();
        }
        if (!configPath.isEmpty()) {
            QJsonObject config = ConfigManager::instance().loadConfig(configPath);
            if (!config.isEmpty()) {
                ConfigManager::instance().applySettingsToConfig(config);
                ConfigManager::instance().saveConfig(configPath, config);
                if (m_kernelService && m_kernelService->isRunning()) {
                    m_kernelService->restartWithConfig(configPath);
                }
            }
        }
    });

    connect(m_homeView, &HomeView::tunModeChanged, this, [this](bool enabled) {
        if (enabled && !AdminHelper::isAdmin()) {
            QMessageBox box(this);
            box.setIcon(QMessageBox::Warning);
            box.setWindowTitle(tr("需要管理员权限"));
            box.setText(tr("切换到 TUN 模式需要以管理员权限重启应用，是否立即以管理员身份重启？"));
            box.addButton(tr("取消"), QMessageBox::RejectRole);
            auto *restartBtn = box.addButton(tr("以管理员身份重启"), QMessageBox::AcceptRole);
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

        if (enabled) {
            SystemProxy::clearProxy();
            AppSettings::instance().setSystemProxyEnabled(false);
            if (m_homeView) {
                m_homeView->setSystemProxyEnabled(false);
            }
        }

        AppSettings::instance().setTunEnabled(enabled);

        QString configPath;
        if (m_subscriptionView && m_subscriptionView->getService()) {
            configPath = m_subscriptionView->getService()->getActiveConfigPath();
        }
        if (configPath.isEmpty()) {
            configPath = ConfigManager::instance().getActiveConfigPath();
        }
        if (!configPath.isEmpty()) {
            QJsonObject config = ConfigManager::instance().loadConfig(configPath);
            if (!config.isEmpty()) {
                ConfigManager::instance().applySettingsToConfig(config);
                ConfigManager::instance().saveConfig(configPath, config);
                if (m_kernelService && m_kernelService->isRunning()) {
                    m_kernelService->restartWithConfig(configPath);
                }
            }
        }
    });

    connect(m_homeView, &HomeView::proxyModeChanged, this, [this](const QString &mode) {
        QString configPath;
        if (m_subscriptionView && m_subscriptionView->getService()) {
            configPath = m_subscriptionView->getService()->getActiveConfigPath();
        }
        if (configPath.isEmpty()) {
            configPath = ConfigManager::instance().getActiveConfigPath();
        }
        if (configPath.isEmpty()) {
            Logger::warn(QStringLiteral(u"\u65e0\u6cd5\u83b7\u53d6\u914d\u7f6e\u6587\u4ef6\u8def\u5f84\uff0c\u4ee3\u7406\u6a21\u5f0f\u5207\u6362\u5931\u8d25"));
            if (m_logView) {
                m_logView->appendLog(QStringLiteral(u"[WARN] \u65e0\u6cd5\u83b7\u53d6\u914d\u7f6e\u6587\u4ef6\u8def\u5f84\uff0c\u4ee3\u7406\u6a21\u5f0f\u5207\u6362\u5931\u8d25"));
            }
            return;
        }

        QString error;
        if (ConfigManager::instance().updateClashDefaultMode(configPath, mode, &error)) {
            const QString msg = QStringLiteral(u"\u4ee3\u7406\u6a21\u5f0f\u5df2\u5207\u6362\u4e3a: %1").arg(mode);
            Logger::info(msg);
            if (m_logView) {
                m_logView->appendLog(QString("[INFO] %1").arg(msg));
            }
            if (m_kernelService && m_kernelService->isRunning()) {
                const QString restartMsg = QStringLiteral(u"\u4ee3\u7406\u6a21\u5f0f\u5207\u6362\u5b8c\u6210\uff0c\u6b63\u5728\u91cd\u542f\u5185\u6838");
                Logger::info(restartMsg);
                if (m_logView) {
                    m_logView->appendLog(QString("[INFO] %1").arg(restartMsg));
                }
                m_kernelService->restartWithConfig(configPath);
            }
        } else {
            const QString msg = QStringLiteral(u"\u5207\u6362\u4ee3\u7406\u6a21\u5f0f\u5931\u8d25: %1").arg(error);
            Logger::error(msg);
            if (m_logView) {
                m_logView->appendLog(QString("[ERROR] %1").arg(msg));
            }
        }
    });

    connect(m_homeView, &HomeView::restartClicked, this, [this]() {
        m_kernelService->restart();
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
    QString configPath;
    if (m_subscriptionView && m_subscriptionView->getService()) {
        configPath = m_subscriptionView->getService()->getActiveConfigPath();
    }
    if (configPath.isEmpty()) {
        configPath = ConfigManager::instance().getActiveConfigPath();
    }
    return configPath;
}

QString MainWindow::currentProxyMode() const
{
    const QString path = activeConfigPath();
    if (path.isEmpty()) return "rule";
    return ConfigManager::instance().readClashDefaultMode(path);
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
        m_startStopBtn->setText(tr("\u505c\u6b62"));
        m_startStopBtn->setStyleSheet(QString(
            "QPushButton { background-color: %1; color: white; border-radius: 10px; border: none; }"
            "QPushButton:hover { background-color: #d32f2f; }"
        ).arg(tm.getColorString("error"))); 
        
        // 鏍稿績鍚姩鍚庡埛鏂颁唬鐞嗗垪琛ㄥ苟寮�濮嬬洃鎺ф祦閲?
        m_proxyService->startTrafficMonitor();
        if (m_homeView && m_homeView->isSystemProxyEnabled()) {
            int port = ConfigManager::instance().getMixedPort();
            SystemProxy::setProxy("127.0.0.1", port);
        }
        QTimer::singleShot(1000, m_proxyView, &ProxyView::refresh);
        QTimer::singleShot(1200, m_rulesView, &RulesView::refresh);
        
    } else {
        m_startStopBtn->setText(tr("\u542f\u52a8"));
        m_startStopBtn->setStyleSheet(tm.getButtonStyle()); // 鎭㈠榛樿涓昏壊鎸夐挳
        
        m_proxyService->stopTrafficMonitor();
        
        // 鍐呮牳鍋滄鏃惰嚜鍔ㄦ竻闄ょ郴缁熶唬鐞?
        SystemProxy::clearProxy();
    }
}

void MainWindow::onStartStopClicked()
{
    if (m_kernelService->isRunning()) {
        m_kernelService->stop();
    } else {
        const QString activeConfig = m_subscriptionView->getService()->getActiveConfigPath();
        if (!activeConfig.isEmpty() && QFile::exists(activeConfig)) {
            m_kernelService->setConfigPath(activeConfig);
            m_kernelService->start(activeConfig);
        } else {
            // 鐢熸垚鏈�鏂伴厤缃?
            ConfigManager::instance().generateConfigWithNodes(QJsonArray());
            m_kernelService->setConfigPath(ConfigManager::instance().getActiveConfigPath());
            m_kernelService->start();
        }
    }
}

void MainWindow::updateStyle()
{
    ThemeManager &tm = ThemeManager::instance();
    
    // 璁剧疆涓荤獥鍙ｈ儗鏅?
    setStyleSheet(QString("#NavContainer { background-color: %1; border-right: 1px solid %2; }"
                          "#ContentContainer { background-color: %3; }")
                  .arg(tm.getColorString("bg-secondary"))
                  .arg(tm.getColorString("border"))
                  .arg(tm.getColorString("bg-primary")));
                  
    // 鏇存柊 Logo 鍜?Version 棰滆壊
    findChild<QLabel*>("LogoLabel")->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: bold; padding: 10px;").arg(tm.getColorString("primary")));
    findChild<QLabel*>("VersionLabel")->setStyleSheet(QString("color: %1;").arg(tm.getColorString("text-tertiary")));
    
    // 瀵艰埅鍒楄〃鏍峰紡浼樺寲
    // 閫変腑椤瑰乏渚у姞涓�鏉¤壊甯︼紝鑳屾櫙鑹插彉娴?
    m_navList->setStyleSheet(QString(R"(
        QListWidget {
            background-color: transparent;
            border: none;
            outline: none;
        }
        QListWidget::item {
            color: %1;
            padding: 12px 20px;
            margin: 4px 12px;
            border-radius: 10px;
            border: none;
        }
        QListWidget::item:hover {
            background-color: %2;
        }
        QListWidget::item:selected {
            background-color: %3;
            color: %4; 
            font-weight: bold;
        }
    )")
    .arg(tm.getColorString("text-secondary"))
    .arg(tm.getColorString("bg-tertiary"))
    .arg("rgba(148, 251, 255, 0.71)") // light yellow highlight
    .arg(tm.getColorString("text-primary")));

    // 鐘舵�佹爮鏍峰紡
    QWidget *statusBar = findChild<QWidget*>("StatusBar");
    if (statusBar) {
        statusBar->setStyleSheet(QString(
            "#StatusBar { background-color: %1; border-top: 1px solid %2; }"
            "QLabel { color: %3; }"
        )
        .arg(tm.getColorString("bg-secondary"))
        .arg(tm.getColorString("border"))
        .arg(tm.getColorString("text-secondary")));
    }
    
    // 鏇存柊鎸夐挳鏍峰紡
    if (!m_kernelService->isRunning()) {
        m_startStopBtn->setStyleSheet(tm.getButtonStyle());
    }
    
    // 瑙﹀彂鎵�鏈夊瓙瑙嗗浘鏇存柊鏍峰紡锛堝鏋滃畠浠洃鍚簡 themeChanged 淇″彿锛屾垨鑰呮垜浠彲浠ユ墜鍔ㄨ皟鐢ㄥ埛鏂版柟娉曪級
    // 鐩墠绠�鍗曡Е鍙戜竴娆￠噸缁樺彲鑳戒笉澶燂紝鏈�濂界殑鏂瑰紡鏄瓙瑙嗗浘涔熻繛鎺?ThemeManager
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
