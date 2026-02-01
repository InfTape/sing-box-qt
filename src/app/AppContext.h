#ifndef APPCONTEXT_H
#define APPCONTEXT_H

#include <memory>

#include "app/interfaces/ConfigRepository.h"
#include "app/interfaces/SettingsStore.h"
#include "app/interfaces/ThemeService.h"
#include "app/interfaces/SystemProxyGateway.h"
#include "app/interfaces/AdminActions.h"

class KernelService;
class ProxyService;
class ProxyController;
class SubscriptionService;

/**
 * @brief AppContext 聚合应用运行期所需的核心服务，统一管理生命周期并便于注入。
 */
class AppContext
{
public:
    AppContext();
    ~AppContext();

    KernelService* kernelService() const;
    ProxyService* proxyService() const;
    ProxyController* proxyController() const;
    SubscriptionService* subscriptionService() const;
    ConfigRepository* configRepository() const;
    SettingsStore* settingsStore() const;
    ThemeService* themeService() const;
    SystemProxyGateway* systemProxy() const;
    AdminActions* adminActions() const;

private:
    std::unique_ptr<KernelService> m_kernelService;
    std::unique_ptr<ProxyService> m_proxyService;
    std::unique_ptr<SubscriptionService> m_subscriptionService;
    std::unique_ptr<ProxyController> m_proxyController;
    std::unique_ptr<ConfigRepository> m_configRepository;
    std::unique_ptr<SettingsStore> m_settingsStore;
    std::unique_ptr<ThemeService> m_themeService;
    std::unique_ptr<SystemProxyGateway> m_systemProxyGateway;
    std::unique_ptr<AdminActions> m_adminActions;
};

#endif // APPCONTEXT_H
