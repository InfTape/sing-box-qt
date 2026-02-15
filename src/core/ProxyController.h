#ifndef PROXYCONTROLLER_H
#define PROXYCONTROLLER_H
#include <QObject>
#include <QString>
class KernelService;
class SubscriptionService;
class ConfigRepository;
class SettingsStore;
class SystemProxyGateway;

class ProxyController : public QObject {
  Q_OBJECT
 public:
  explicit ProxyController(KernelService*       kernel,
                           SubscriptionService* subscription = nullptr,
                           ConfigRepository*    configRepo   = nullptr,
                           SettingsStore*       settings     = nullptr,
                           SystemProxyGateway*  systemProxy  = nullptr,
                           QObject*             parent       = nullptr);
  void    setSubscriptionService(SubscriptionService* service);
  QString activeConfigPath() const;
  QString currentProxyMode() const;
  bool    startKernel();
  void    stopKernel();
  bool    toggleKernel();
  bool    setProxyMode(const QString& mode,
                       bool           restartIfRunning,
                       QString*       error = nullptr);
  bool    restartKernelWithConfig(const QString& configPath);
  bool    setSystemProxyEnabled(bool enabled);
  bool    setTunModeEnabled(bool enabled, bool restartIfRunning = true);
  bool    syncSettingsToActiveConfig(bool restartIfRunning = false);
  void    updateSystemProxyForKernelState(bool running);

  KernelService* kernel() const { return m_kernel; }

 private:
  bool           ensureConfigExists(QString* outPath = nullptr);
  bool           applySettingsToActiveConfig(bool restartIfRunning = true);
  KernelService* m_kernel;
  SubscriptionService* m_subscription;
  ConfigRepository*    m_configRepo;
  SettingsStore*       m_settings;
  SystemProxyGateway*  m_systemProxy;
};
#endif  // PROXYCONTROLLER_H
