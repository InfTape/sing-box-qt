#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QStackedWidget>
#include <memory>
class HomeView;
class ProxyView;
class SubscriptionView;
class ConnectionsView;
class RulesView;
class LogView;
class SettingsView;
class SubscriptionService;
class ProxyController;
class ProxyUiController;
class ProxyRuntimeController;
class SettingsStore;
class ThemeService;
class AdminActions;
class SettingsController;
class RuntimeUiBinder;
class AppContext;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(AppContext& ctx, QWidget* parent = nullptr);
  ~MainWindow();
  void    showAndActivate();
  bool    isKernelRunning() const;
  QString currentProxyMode() const;
  QString activeConfigPath() const;

  ProxyController* proxyController() const { return m_proxyController; }

  void setProxyModeUI(const QString& mode);

 protected:
  void closeEvent(QCloseEvent* event) override;
 private slots:
  void onNavigationItemClicked(QListWidgetItem* item);
  void onKernelStatusChanged(bool running);
  void onStartStopClicked();

 private:
  void setupUI();
  void setupNavigation();
  void setupStatusBar();
  void setupConnections();
  void setupNavigationConnections();
  void setupThemeConnections();
  void setupSubscriptionConnections();
  void setupHomeViewConnections();
  void setupProxyUiBindings();
  void loadSettings();
  void saveSettings();
  void updateStyle();
  void updateNavIcons();
  void applyStartStopStyle();
  void showStopFailedToast();
  // UI components.
  QWidget*        m_centralWidget;
  QListWidget*    m_navList;
  QStackedWidget* m_stackedWidget;
  // Status bar components.
  QPushButton* m_startStopBtn;
  // Views.
  HomeView*         m_homeView;
  ProxyView*        m_proxyView;
  SubscriptionView* m_subscriptionView;
  ConnectionsView*  m_connectionsView;
  RulesView*        m_rulesView;
  LogView*          m_logView;
  SettingsView*     m_settingsView;
  // Services.
  AppContext&                      m_ctx;
  ProxyController*                 m_proxyController;
  ProxyUiController*               m_proxyUiController;
  ProxyRuntimeController*          m_proxyRuntimeController;
  SubscriptionService*             m_subscriptionService;
  SettingsStore*                   m_settingsStore;
  ThemeService*                    m_themeService;
  AdminActions*                    m_adminActions;
  SettingsController*              m_settingsController;
  std::unique_ptr<RuntimeUiBinder> m_runtimeBinder;
  int                              m_stopCheckSeq = 0;
};
#endif  // MAINWINDOW_H
