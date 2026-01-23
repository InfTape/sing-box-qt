#include "TrayIcon.h"
#include "MainWindow.h"
#include "core/KernelService.h"
#include "utils/ThemeManager.h"
#include "storage/ConfigManager.h"
#include <QMessageBox>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>

class RoundedMenu : public QMenu
{
public:
    explicit RoundedMenu(QWidget *parent = nullptr)
        : QMenu(parent)
    {
        setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
    }

    void setThemeColors(const QColor &bg, const QColor &border)
    {
        m_bgColor = bg;
        m_borderColor = border;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QRectF rect = this->rect().adjusted(1, 1, -1, -1);
        QPainterPath path;
        path.addRoundedRect(rect, 10, 10);

        painter.fillPath(path, m_bgColor);
        painter.setPen(QPen(m_borderColor, 1));
        painter.drawPath(path);

        QMenu::paintEvent(event);
    }

private:
    QColor m_bgColor = QColor(30, 41, 59);
    QColor m_borderColor = QColor(255, 255, 255, 26);
};

TrayIcon::TrayIcon(MainWindow *mainWindow, QObject *parent)
    : QSystemTrayIcon(parent)
    , m_mainWindow(mainWindow)
{
    // 设置图标
    setIcon(QIcon(":/icons/app.png"));
    setToolTip(tr("Sing-Box - 已停止"));
    
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
    
    m_showAction = m_menu->addAction(tr("显示主窗口"));
    connect(m_showAction, &QAction::triggered, this, &TrayIcon::onShowWindow);
    
    m_menu->addSeparator();
    
    m_toggleAction = m_menu->addAction(tr("启动代理"));
    m_toggleAction->setCheckable(true);
    connect(m_toggleAction, &QAction::triggered, this, &TrayIcon::onToggleProxy);

    m_menu->addSeparator();

    m_globalAction = m_menu->addAction(tr("全局模式"));
    m_ruleAction = m_menu->addAction(tr("规则模式"));
    m_globalAction->setCheckable(true);
    m_ruleAction->setCheckable(true);
    connect(m_globalAction, &QAction::triggered, this, &TrayIcon::onSelectGlobal);
    connect(m_ruleAction, &QAction::triggered, this, &TrayIcon::onSelectRule);

    m_menu->addSeparator();
    
    m_quitAction = m_menu->addAction(tr("退出"));
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
    const bool running = m_mainWindow->isKernelRunning();
    auto *kernel = m_mainWindow->kernelService();
    if (!kernel) return;

    if (running) {
        kernel->stop();
    } else {
        QString configPath = m_mainWindow->activeConfigPath();
        if (configPath.isEmpty()) {
            QMessageBox::warning(nullptr, tr("启动代理"), tr("未找到配置文件，无法启动内核。"));
            return;
        }
        kernel->start(configPath);
    }
    updateProxyStatus();
}

void TrayIcon::onSelectGlobal()
{
    const QString mode = "global";
    const QString configPath = m_mainWindow->activeConfigPath();
    if (configPath.isEmpty()) return;
    QString error;
    if (ConfigManager::instance().updateClashDefaultMode(configPath, mode, &error)) {
        if (m_mainWindow->isKernelRunning()) {
            m_mainWindow->kernelService()->restartWithConfig(configPath);
        }
    } else {
        QMessageBox::warning(nullptr, tr("切换模式失败"), error);
    }
    m_mainWindow->setProxyModeUI(mode);
    updateProxyStatus();
}

void TrayIcon::onSelectRule()
{
    const QString mode = "rule";
    const QString configPath = m_mainWindow->activeConfigPath();
    if (configPath.isEmpty()) return;
    QString error;
    if (ConfigManager::instance().updateClashDefaultMode(configPath, mode, &error)) {
        if (m_mainWindow->isKernelRunning()) {
            m_mainWindow->kernelService()->restartWithConfig(configPath);
        }
    } else {
        QMessageBox::warning(nullptr, tr("切换模式失败"), error);
    }
    m_mainWindow->setProxyModeUI(mode);
    updateProxyStatus();
}

void TrayIcon::onQuit()
{
    // TODO: 停止内核进程
    QApplication::quit();
}

void TrayIcon::updateProxyStatus()
{
    const bool running = m_mainWindow->isKernelRunning();
    m_toggleAction->setChecked(running);
    m_toggleAction->setText(running ? tr("关闭代理") : tr("启动代理"));
    setToolTip(running ? tr("Sing-Box - 运行中") : tr("Sing-Box - 已停止"));

    const QString mode = m_mainWindow->currentProxyMode();
    m_globalAction->setChecked(mode == "global");
    m_ruleAction->setChecked(mode != "global");
    m_mainWindow->setProxyModeUI(mode);
}
