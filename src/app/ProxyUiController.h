#ifndef PROXYUICONTROLLER_H
#define PROXYUICONTROLLER_H

#include <QObject>
#include <functional>

class ProxyController;
class KernelService;
class SettingsStore;
class AdminActions;
/**
 * @brief ProxyUiController
 * 负责 UI 层触发的代理相关动作与业务服务之间的协调，
 * 将系统代理 / TUN / 代理模式切换等流程从视图中抽离。
 */
class ProxyUiController : public QObject {
  Q_OBJECT
 public:
  enum class TunResult { Applied, Cancelled, Failed };

  explicit ProxyUiController(ProxyController* proxyController,
                             KernelService*   kernelService,
                             SettingsStore*   settingsStore,
                             AdminActions*    adminActions,
                             QObject*         parent = nullptr);

  bool    isKernelRunning() const;
  QString currentProxyMode() const;
  bool    systemProxyEnabled() const;
  bool    tunModeEnabled() const;

  bool      toggleKernel(QString* error = nullptr);
  bool      switchProxyMode(const QString& mode, QString* error = nullptr);
  bool      setSystemProxyEnabled(bool enabled, QString* error = nullptr);
  TunResult setTunModeEnabled(
      bool enabled, const std::function<bool()>& confirmRestartAdmin = {});

  void broadcastCurrentStates();

 signals:
  void systemProxyStateChanged(bool enabled);
  void tunModeStateChanged(bool enabled);
  void proxyModeChanged(const QString& mode);

 private:
  ProxyController* m_proxyController = nullptr;
  KernelService*   m_kernelService   = nullptr;
  SettingsStore*   m_settings        = nullptr;
  AdminActions*    m_adminActions    = nullptr;
};
#endif  // PROXYUICONTROLLER_H
