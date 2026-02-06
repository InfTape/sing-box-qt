#include "AppContext.h"
#include "app/ProxyRuntimeController.h"
#include "app/ProxyUiController.h"
#include "app/impl/AdminActionsAdapter.h"
#include "app/impl/ConfigRepositoryAdapter.h"
#include "app/impl/SettingsStoreAdapter.h"
#include "app/impl/SystemProxyAdapter.h"
#include "app/impl/ThemeServiceAdapter.h"
#include "core/KernelService.h"
#include "core/ProxyController.h"
#include "core/ProxyService.h"
#include "network/SubscriptionService.h"
#include "views/proxy/ProxyViewController.h"
AppContext::AppContext() {
  m_configRepository    = std::make_unique<ConfigRepositoryAdapter>();
  m_kernelService       = std::make_unique<KernelService>();
  m_proxyService        = std::make_unique<ProxyService>();
  m_subscriptionService = std::make_unique<SubscriptionService>(m_configRepository.get());
  m_settingsStore       = std::make_unique<SettingsStoreAdapter>();
  m_themeService        = std::make_unique<ThemeServiceAdapter>();
  m_systemProxyGateway  = std::make_unique<SystemProxyAdapter>();
  m_adminActions        = std::make_unique<AdminActionsAdapter>();
  m_proxyController =
      std::make_unique<ProxyController>(m_kernelService.get(), m_subscriptionService.get(), m_configRepository.get(),
                                        m_settingsStore.get(), m_systemProxyGateway.get());
  m_proxyUiController = std::make_unique<ProxyUiController>(m_proxyController.get(), m_kernelService.get(),
                                                            m_settingsStore.get(), m_adminActions.get());
  m_proxyRuntimeController =
      std::make_unique<ProxyRuntimeController>(m_kernelService.get(), m_proxyService.get(), m_proxyController.get());
  m_proxyViewController = std::make_unique<ProxyViewController>(m_configRepository.get());
  m_proxyViewController->setProxyService(m_proxyService.get());
}
AppContext::~AppContext() = default;
KernelService* AppContext::kernelService() const {
  return m_kernelService.get();
}
ProxyService* AppContext::proxyService() const {
  return m_proxyService.get();
}
ProxyController* AppContext::proxyController() const {
  return m_proxyController.get();
}
ProxyUiController* AppContext::proxyUiController() const {
  return m_proxyUiController.get();
}
ProxyRuntimeController* AppContext::proxyRuntimeController() const {
  return m_proxyRuntimeController.get();
}
ProxyViewController* AppContext::proxyViewController() const {
  return m_proxyViewController.get();
}
SubscriptionService* AppContext::subscriptionService() const {
  return m_subscriptionService.get();
}
ConfigRepository* AppContext::configRepository() const {
  return m_configRepository.get();
}
SettingsStore* AppContext::settingsStore() const {
  return m_settingsStore.get();
}
ThemeService* AppContext::themeService() const {
  return m_themeService.get();
}
SystemProxyGateway* AppContext::systemProxy() const {
  return m_systemProxyGateway.get();
}
AdminActions* AppContext::adminActions() const {
  return m_adminActions.get();
}
