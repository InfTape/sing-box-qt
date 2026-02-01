#include "TrayIcon.h"
#include "app/ProxyUiController.h"
#include "core/KernelService.h"
#include "app/interfaces/ThemeService.h"
#include "widgets/common/RoundedMenu.h"
#include <QMessageBox>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <utility>

TrayIcon::TrayIcon(ProxyUiController *proxyUiController,
                   KernelService *kernelService,
                   ThemeService *themeService,
                   std::function<void()> showWindow,
                   QObject *parent)
    : QSystemTrayIcon(parent)
    , m_proxyUiController(proxyUiController)
    , m_kernelService(kernelService)
    , m_showWindow(std::move(showWindow))
    , m_themeService(themeService)
{
    setIcon(QIcon(":/icons/app.png"));
    setToolTip(tr("Sing-Box - Stopped"));
    
    setupMenu();
    
    connect(this, &QSystemTrayIcon::activated,
            this, &TrayIcon::onActivated);

    if (m_kernelService) {
        connect(m_kernelService, &KernelService::statusChanged,
                this, [this](bool){ updateProxyStatus(); });
    }
    if (m_proxyUiController) {
        connect(m_proxyUiController, &ProxyUiController::proxyModeChanged,
                this, [this](const QString &){ updateProxyStatus(); });
    }
}

TrayIcon::~TrayIcon()
{
    delete m_menu;
}

void TrayIcon::setupMenu()
{
    auto *menu = new RoundedMenu;
    menu->setObjectName("TrayMenu");
    ThemeService *ts = m_themeService;
    if (ts) {
        menu->setThemeColors(ts->color("bg-secondary"), ts->color("primary"));
        connect(ts, &ThemeService::themeChanged, menu, [menu, ts]() {
            menu->setThemeColors(ts->color("bg-secondary"), ts->color("primary"));
        });
    }

    m_menu = menu;
    
    m_showAction = m_menu->addAction(tr("Show Window"));
    connect(m_showAction, &QAction::triggered, this, &TrayIcon::onShowWindow);
    
    m_menu->addSeparator();
    
    m_toggleAction = m_menu->addAction(tr("Start/Stop Proxy"));
    connect(m_toggleAction, &QAction::triggered, this, &TrayIcon::onToggleProxy);

    m_menu->addSeparator();

    m_globalAction = m_menu->addAction(tr("Global Mode"));
    m_ruleAction = m_menu->addAction(tr("Rule Mode"));
    m_globalAction->setCheckable(true);
    m_ruleAction->setCheckable(true);
    connect(m_globalAction, &QAction::triggered, this, &TrayIcon::onSelectGlobal);
    connect(m_ruleAction, &QAction::triggered, this, &TrayIcon::onSelectRule);

    m_menu->addSeparator();
    
    m_quitAction = m_menu->addAction(tr("Quit"));
    connect(m_quitAction, &QAction::triggered, this, &TrayIcon::onQuit);

    connect(m_menu, &QMenu::aboutToShow, this, &TrayIcon::updateProxyStatus);
    setContextMenu(m_menu);
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        onShowWindow();
        break;
    default:
        break;
    }
}

void TrayIcon::onShowWindow()
{
    if (m_showWindow) {
        m_showWindow();
    }
}

void TrayIcon::onToggleProxy()
{
    if (!m_proxyUiController) return;
    QString error;
    if (!m_proxyUiController->toggleKernel(&error)) {
        QMessageBox::warning(nullptr, tr("Start Proxy"),
                             error.isEmpty() ? tr("Configuration file not found. Unable to start kernel.")
                                             : error);
    }
    updateProxyStatus();
}

void TrayIcon::onSelectGlobal()
{
    if (!m_proxyUiController) return;

    QString error;
    if (!m_proxyUiController->switchProxyMode("global", &error)) {
        QMessageBox::warning(nullptr, tr("Switch Mode Failed"),
                             error.isEmpty() ? tr("Failed to switch proxy mode") : error);
        return;
    }
    updateProxyStatus();
}

void TrayIcon::onSelectRule()
{
    if (!m_proxyUiController) return;

    QString error;
    if (!m_proxyUiController->switchProxyMode("rule", &error)) {
        QMessageBox::warning(nullptr, tr("Switch Mode Failed"),
                             error.isEmpty() ? tr("Failed to switch proxy mode") : error);
        return;
    }
    updateProxyStatus();
}

void TrayIcon::onQuit()
{
    // TODO: stop the kernel process
    QApplication::quit();
}

void TrayIcon::updateProxyStatus()
{
    const bool running = m_proxyUiController ? m_proxyUiController->isKernelRunning()
                                             : (m_kernelService && m_kernelService->isRunning());
    m_toggleAction->setText(running ? tr("Stop Proxy") : tr("Start Proxy"));
    setToolTip(running ? tr("Sing-Box - Running") : tr("Sing-Box - Stopped"));

    QString mode = "rule";
    if (m_proxyUiController) {
        mode = m_proxyUiController->currentProxyMode();
    }
    m_globalAction->setChecked(mode == "global");
    m_ruleAction->setChecked(mode != "global");
}
