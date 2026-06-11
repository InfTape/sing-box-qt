#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
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

 private:
  void setupUI();
  void setupNavigation();
  void setupConnections();
  void setupNavigationConnections();
  void setupThemeConnections();
  void setupSubscriptionConnections();
  void setupHomeViewConnections();
  void setupProxyUiBindings();
  void loadSettings();
  void saveSettings();
  void openSettingsPage();
  void updateStyle();
  void updateNavIcons();
  void restoreRuntimeOnStartup();
  // UI components.
  QWidget*        m_centralWidget;
  QListWidget*    m_navList;
  QStackedWidget* m_stackedWidget;
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
};
#endif  // MAINWINDOW_H
