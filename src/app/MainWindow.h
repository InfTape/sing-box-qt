#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>

class HomeView;
class ProxyView;
class SubscriptionView;
class ConnectionsView;
class RulesView;
class LogView;
class SettingsView;
class KernelService;
class ProxyService;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void showAndActivate();
    bool isKernelRunning() const;
    QString currentProxyMode() const;
    QString activeConfigPath() const;
    KernelService* kernelService() const { return m_kernelService; }
    void setProxyModeUI(const QString &mode);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onNavigationItemClicked(QListWidgetItem *item);
    void onKernelStatusChanged(bool running);
    void onStartStopClicked();

private:
    void setupUI();
    void setupNavigation();
    void setupStatusBar();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void updateStyle();

    // UI 组件
    QWidget *m_centralWidget;
    QListWidget *m_navList;
    QStackedWidget *m_stackedWidget;
    
    // 状态栏组件
    QPushButton *m_startStopBtn;

    // 视图
    HomeView *m_homeView;
    ProxyView *m_proxyView;
    SubscriptionView *m_subscriptionView;
    ConnectionsView *m_connectionsView;
    RulesView *m_rulesView;
    LogView *m_logView;
    SettingsView *m_settingsView;

    // 服务
    KernelService *m_kernelService;
    ProxyService *m_proxyService;
};

#endif // MAINWINDOW_H
