#include "AppContext.h"

#include "core/KernelService.h"
#include "core/ProxyService.h"
#include "core/ProxyController.h"
#include "network/SubscriptionService.h"
#include "app/impl/ConfigRepositoryAdapter.h"
#include "app/impl/SettingsStoreAdapter.h"

AppContext::AppContext()
{
    m_kernelService = std::make_unique<KernelService>();
    m_proxyService = std::make_unique<ProxyService>();
    m_subscriptionService = std::make_unique<SubscriptionService>();
    m_configRepository = std::make_unique<ConfigRepositoryAdapter>();
    m_settingsStore = std::make_unique<SettingsStoreAdapter>();
    m_proxyController = std::make_unique<ProxyController>(m_kernelService.get(),
                                                          m_subscriptionService.get(),
                                                          m_configRepository.get(),
                                                          m_settingsStore.get());
}

AppContext::~AppContext() = default;

KernelService* AppContext::kernelService() const
{
    return m_kernelService.get();
}

ProxyService* AppContext::proxyService() const
{
    return m_proxyService.get();
}

ProxyController* AppContext::proxyController() const
{
    return m_proxyController.get();
}

SubscriptionService* AppContext::subscriptionService() const
{
    return m_subscriptionService.get();
}

ConfigRepository* AppContext::configRepository() const
{
    return m_configRepository.get();
}

SettingsStore* AppContext::settingsStore() const
{
    return m_settingsStore.get();
}
