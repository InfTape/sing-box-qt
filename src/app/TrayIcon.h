#ifndef TRAYICON_H
#define TRAYICON_H

#include <QSystemTrayIcon>
#include <QMenu>

class MainWindow;

class TrayIcon : public QSystemTrayIcon
{
    Q_OBJECT

public:
    explicit TrayIcon(MainWindow *mainWindow, QObject *parent = nullptr);
    ~TrayIcon();

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void onShowWindow();
    void onToggleProxy();
    void onSelectGlobal();
    void onSelectRule();
    void onQuit();

private:
    void setupMenu();
    void updateProxyStatus();

    MainWindow *m_mainWindow;
    QMenu *m_menu;
    QAction *m_toggleAction;
    QAction *m_globalAction;
    QAction *m_ruleAction;
    QAction *m_showAction;
    QAction *m_quitAction;
};

#endif // TRAYICON_H
