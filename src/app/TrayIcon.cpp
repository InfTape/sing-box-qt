#include "TrayIcon.h"
#include "MainWindow.h"
#include "core/KernelService.h"
#include "core/ProxyController.h"
#include "utils/ThemeManager.h"
#include "storage/ConfigManager.h"
#include "widgets/RoundedMenu.h"
#include <QMessageBox>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>

TrayIcon::TrayIcon(MainWindow *mainWindow, QObject *parent)
    : QSystemTrayIcon(parent)
    , m_mainWindow(mainWindow)
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
    ThemeManager &tm = ThemeManager::instance();
    menu->setThemeColors(tm.getColor("bg-secondary"), tm.getColor("primary"));
    connect(&tm, &ThemeManager::themeChanged, menu, [menu, &tm]() {
        menu->setThemeColors(tm.getColor("bg-secondary"), tm.getColor("primary"));
        menu->setStyleSheet(QString(R"(
            #TrayMenu {
                background: transparent;
                border: none;
                padding: 6px;
            }
            #TrayMenu::panel {
                background: transparent;
                border: none;
            }
            #TrayMenu::item {
                color: %1;
                padding: 8px 14px;
                border-radius: 10px;
            }
            #TrayMenu::indicator {
                width: 14px;
                height: 14px;
            }
            #TrayMenu::indicator:checked {
                image: url(:/icons/check.svg);
            }
            #TrayMenu::indicator:unchecked {
                image: none;
            }
            #TrayMenu::item:selected {
                background-color: %2;
                color: %1;
            }
            #TrayMenu::separator {
                height: 1px;
                background-color: %3;
                margin: 6px 4px;
            }
        )")
        .arg(tm.getColorString("text-primary"))
        .arg(tm.getColorString("bg-tertiary"))
        .arg(tm.getColorString("border")));
    });

    menu->setStyleSheet(QString(R"(
        #TrayMenu {
            background: transparent;
            border: none;
            padding: 6px;
        }
        #TrayMenu::panel {
            background: transparent;
            border: none;
        }
        #TrayMenu::item {
            color: %1;
            padding: 8px 14px;
            border-radius: 10px;
        }
        #TrayMenu::indicator {
            width: 14px;
            height: 14px;
        }
        #TrayMenu::indicator:checked {
            image: url(:/icons/check.svg);
        }
        #TrayMenu::indicator:unchecked {
            image: none;
        }
        #TrayMenu::item:selected {
            background-color: %2;
            color: %1;
        }
        #TrayMenu::separator {
            height: 1px;
            background-color: %3;
            margin: 6px 4px;
        }
    )")
    .arg(tm.getColorString("text-primary"))
    .arg(tm.getColorString("bg-tertiary"))
    .arg(tm.getColorString("border")));

    m_menu = menu;
    
    m_showAction = m_menu->addAction(tr("Show Window"));
    connect(m_showAction, &QAction::triggered, this, &TrayIcon::onShowWindow);
    
    m_menu->addSeparator();
    
    m_toggleAction = m_menu->addAction(tr("Start/Stop Proxy"));
    m_toggleAction->setCheckable(true);
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
    m_toggleAction->setChecked(running);
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
