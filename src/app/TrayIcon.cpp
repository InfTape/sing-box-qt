#include "TrayIcon.h"
#include "MainWindow.h"
#include "core/KernelService.h"
#include "core/ProxyController.h"
#include "app/interfaces/ThemeService.h"
#include "services/config/ConfigManager.h"
#include "widgets/common/RoundedMenu.h"
#include <QMessageBox>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>

TrayIcon::TrayIcon(MainWindow *mainWindow, ThemeService *themeService, QObject *parent)
    : QSystemTrayIcon(parent)
    , m_mainWindow(mainWindow)
    , m_themeService(themeService)
{
    setIcon(QIcon(":/icons/app.png"));
    setToolTip(tr("Sing-Box - Stopped"));
    
    setupMenu();
    
    connect(this, &QSystemTrayIcon::activated,
            this, &TrayIcon::onActivated);

    if (m_mainWindow && m_mainWindow->kernelService()) {
        connect(m_mainWindow->kernelService(), &KernelService::statusChanged,
                this, [this](bool){ updateProxyStatus(); });
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
    m_mainWindow->showAndActivate();
}

void TrayIcon::onToggleProxy()
{
    if (!m_mainWindow) return;
    auto *controller = m_mainWindow->proxyController();
    if (!controller) return;

    if (!controller->toggleKernel()) {
        QMessageBox::warning(nullptr, tr("Start Proxy"), tr("Configuration file not found. Unable to start kernel."));
    }
    updateProxyStatus();
}

void TrayIcon::onSelectGlobal()
{
    auto *controller = m_mainWindow ? m_mainWindow->proxyController() : nullptr;
    if (!controller) return;

    QString error;
    if (!controller->setProxyMode("global", m_mainWindow->isKernelRunning(), &error)) {
        QMessageBox::warning(nullptr, tr("Switch Mode Failed"), error);
        return;
    }
    m_mainWindow->setProxyModeUI("global");
    updateProxyStatus();
}

void TrayIcon::onSelectRule()
{
    auto *controller = m_mainWindow ? m_mainWindow->proxyController() : nullptr;
    if (!controller) return;

    QString error;
    if (!controller->setProxyMode("rule", m_mainWindow->isKernelRunning(), &error)) {
        QMessageBox::warning(nullptr, tr("Switch Mode Failed"), error);
        return;
    }
    m_mainWindow->setProxyModeUI("rule");
    updateProxyStatus();
}

void TrayIcon::onQuit()
{
    // TODO: stop the kernel process
    QApplication::quit();
}

void TrayIcon::updateProxyStatus()
{
    if (!m_mainWindow) return;
    const bool running = m_mainWindow->isKernelRunning();
    m_toggleAction->setText(running ? tr("Stop Proxy") : tr("Start Proxy"));
    setToolTip(running ? tr("Sing-Box - Running") : tr("Sing-Box - Stopped"));

    QString mode = "rule";
    if (auto *controller = m_mainWindow->proxyController()) {
        mode = controller->currentProxyMode();
    } else {
        mode = m_mainWindow->currentProxyMode();
    }
    m_globalAction->setChecked(mode == "global");
    m_ruleAction->setChecked(mode != "global");
    m_mainWindow->setProxyModeUI(mode);
}
