#ifndef TRAYICON_H
#define TRAYICON_H

#include <QMenu>
#include <QSystemTrayIcon>
#include <functional>

class KernelService;
class ProxyUiController;
class ThemeService;
class TrayIcon : public QSystemTrayIcon {
  Q_OBJECT

 public:
  explicit TrayIcon(ProxyUiController* proxyUiController,
                    KernelService* kernelService, ThemeService* themeService,
                    std::function<void()> showWindow,
                    QObject*              parent = nullptr);
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

  ProxyUiController*    m_proxyUiController = nullptr;
  KernelService*        m_kernelService     = nullptr;
  std::function<void()> m_showWindow;
  QMenu*                m_menu;
  QAction*              m_toggleAction;
  QAction*              m_globalAction;
  QAction*              m_ruleAction;
  QAction*              m_showAction;
  QAction*              m_quitAction;
  ThemeService*         m_themeService = nullptr;
};
#endif  // TRAYICON_H
