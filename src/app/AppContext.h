#ifndef APPCONTEXT_H
#define APPCONTEXT_H

#include <memory>

#include "app/interfaces/AdminActions.h"
#include "app/interfaces/ConfigRepository.h"
#include "app/interfaces/SettingsStore.h"
#include "app/interfaces/SystemProxyGateway.h"
#include "app/interfaces/ThemeService.h"

class KernelService;
class ProxyService;
class ProxyController;
class ProxyUiController;
class ProxyRuntimeController;
class ProxyViewController;
class SubscriptionService;
/**
 * @brief AppContext aggregates core services required during runtime, managing lifecycle and injection.
 */
class AppContext {
 public:
  AppContext();
  ~AppContext();

  KernelService*          kernelService() const;
  ProxyService*           proxyService() const;
  ProxyController*        proxyController() const;
  ProxyUiController*      proxyUiController() const;
  ProxyRuntimeController* proxyRuntimeController() const;
  ProxyViewController*    proxyViewController() const;
  SubscriptionService*    subscriptionService() const;
  ConfigRepository*       configRepository() const;
  SettingsStore*          settingsStore() const;
  ThemeService*           themeService() const;
  SystemProxyGateway*     systemProxy() const;
  AdminActions*           adminActions() const;

 private:
  std::unique_ptr<KernelService>          m_kernelService;
  std::unique_ptr<ProxyService>           m_proxyService;
  std::unique_ptr<SubscriptionService>    m_subscriptionService;
  std::unique_ptr<ProxyController>        m_proxyController;
  std::unique_ptr<ProxyUiController>      m_proxyUiController;
  std::unique_ptr<ProxyRuntimeController> m_proxyRuntimeController;
  std::unique_ptr<ProxyViewController>    m_proxyViewController;
  std::unique_ptr<ConfigRepository>       m_configRepository;
  std::unique_ptr<SettingsStore>          m_settingsStore;
  std::unique_ptr<ThemeService>           m_themeService;
  std::unique_ptr<SystemProxyGateway>     m_systemProxyGateway;
  std::unique_ptr<AdminActions>           m_adminActions;
};
#endif  // APPCONTEXT_H
