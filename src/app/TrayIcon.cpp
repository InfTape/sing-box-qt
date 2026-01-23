#include "TrayIcon.h"
#include "MainWindow.h"
#include "utils/ThemeManager.h"
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
    connect(m_toggleAction, &QAction::triggered, this, &TrayIcon::onToggleProxy);
    
    m_menu->addSeparator();
    
    m_quitAction = m_menu->addAction(tr("退出"));
    connect(m_quitAction, &QAction::triggered, this, &TrayIcon::onQuit);
    
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
    // TODO: 实现代理切换
}

void TrayIcon::onQuit()
{
    // TODO: 停止内核进程
    QApplication::quit();
}

void TrayIcon::updateProxyStatus(bool enabled)
{
    if (enabled) {
        setToolTip(tr("Sing-Box - 运行中"));
        m_toggleAction->setText(tr("停止代理"));
    } else {
        setToolTip(tr("Sing-Box - 已停止"));
        m_toggleAction->setText(tr("启动代理"));
    }
}
