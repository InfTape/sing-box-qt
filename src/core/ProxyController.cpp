#include "ProxyController.h"
#include "KernelService.h"
#include "app/ConfigProvider.h"
#include "storage/AppSettings.h"
#include "network/SubscriptionService.h"
#include "system/SystemProxy.h"
#include "utils/Logger.h"
#include <QFile>
#include <QObject>
#include <QJsonArray>
#include <QJsonObject>

#include "app/interfaces/ConfigRepository.h"
#include "app/interfaces/SettingsStore.h"
#include "app/interfaces/SystemProxyGateway.h"

ProxyController::ProxyController(KernelService *kernel,
                                 SubscriptionService *subscription,
                                 ConfigRepository *configRepo,
                                 SettingsStore *settings,
                                 SystemProxyGateway *systemProxy,
                                 QObject *parent)
    : QObject(parent)
    , m_kernel(kernel)
    , m_subscription(subscription)
    , m_configRepo(configRepo)
    , m_settings(settings)
    , m_systemProxy(systemProxy)
{
}

void ProxyController::setSubscriptionService(SubscriptionService *service)
{
    m_subscription = service;
}

QString ProxyController::activeConfigPath() const
{
    QString configPath;
    if (m_subscription) {
        configPath = m_subscription->getActiveConfigPath();
    }
    if (configPath.isEmpty()) {
        ConfigRepository *repo = m_configRepo ? m_configRepo : ConfigProvider::instance();
        if (repo) configPath = repo->getActiveConfigPath();
    }
    return configPath;
}

QString ProxyController::currentProxyMode() const
{
    const QString path = activeConfigPath();
    if (path.isEmpty()) return "rule";
    ConfigRepository *repo = m_configRepo ? m_configRepo : ConfigProvider::instance();
    return repo ? repo->readClashDefaultMode(path) : "rule";
}

bool ProxyController::ensureConfigExists(QString *outPath)
{
    QString configPath = activeConfigPath();
    if (!configPath.isEmpty() && QFile::exists(configPath)) {
        if (outPath) *outPath = configPath;
        return true;
    }

    ConfigRepository *repo = m_configRepo ? m_configRepo : ConfigProvider::instance();
    if (!repo) {
        Logger::error(QStringLiteral("Config repository not available"));
        return false;
    }
    if (!repo->generateConfigWithNodes(QJsonArray())) {
        Logger::error(QStringLiteral("Failed to generate default config"));
        return false;
    }
    configPath = repo->getActiveConfigPath();
    if (outPath) *outPath = configPath;
    return QFile::exists(configPath);
}

bool ProxyController::startKernel()
{
    if (!m_kernel) return false;

    QString configPath;
    if (!ensureConfigExists(&configPath)) {
        return false;
    }

    m_kernel->setConfigPath(configPath);
    return m_kernel->start(configPath);
}

void ProxyController::stopKernel()
{
    if (m_kernel) {
        m_kernel->stop();
    }
}

bool ProxyController::toggleKernel()
{
    if (!m_kernel) return false;
    if (m_kernel->isRunning()) {
        m_kernel->stop();
        return true;
    }
    return startKernel();
}

bool ProxyController::setProxyMode(const QString &mode, bool restartIfRunning, QString *error)
{
    const QString configPath = activeConfigPath();
    if (configPath.isEmpty()) {
        if (error) *error = QObject::tr("Failed to resolve config path");
        return false;
    }

    ConfigRepository *repo = m_configRepo ? m_configRepo : ConfigProvider::instance();
    if (!repo) {
        if (error) *error = QObject::tr("Config repository not available");
        return false;
    }
    bool ok = repo->updateClashDefaultMode(configPath, mode, error);
    if (ok && restartIfRunning && m_kernel && m_kernel->isRunning()) {
        m_kernel->restartWithConfig(configPath);
    }
    return ok;
}

bool ProxyController::restartKernelWithConfig(const QString &configPath)
{
    if (!m_kernel || configPath.isEmpty()) return false;
    m_kernel->setConfigPath(configPath);
    if (m_kernel->isRunning()) {
        m_kernel->restartWithConfig(configPath);
    } else {
        m_kernel->start(configPath);
    }
    return true;
}

bool ProxyController::setSystemProxyEnabled(bool enabled)
{
    SettingsStore *settings = m_settings;
    if (enabled) {
        ConfigRepository *repo = m_configRepo ? m_configRepo : ConfigProvider::instance();
        const int port = repo ? repo->mixedPort() : 1080;
        if (m_systemProxy) {
            m_systemProxy->setProxy("127.0.0.1", port);
        } else {
            SystemProxy::setProxy("127.0.0.1", port);
        }
        if (settings) {
            settings->setSystemProxyEnabled(true);
            settings->setTunEnabled(false);
        } else {
            AppSettings::instance().setSystemProxyEnabled(true);
            AppSettings::instance().setTunEnabled(false);
        }
    } else {
        if (m_systemProxy) {
            m_systemProxy->clearProxy();
        } else {
            SystemProxy::clearProxy();
        }
        if (settings) {
            settings->setSystemProxyEnabled(false);
        } else {
            AppSettings::instance().setSystemProxyEnabled(false);
        }
    }
    return applySettingsToActiveConfig(true);
}

bool ProxyController::setTunModeEnabled(bool enabled)
{
    if (enabled) {
        if (m_systemProxy) {
            m_systemProxy->clearProxy();
        } else {
            SystemProxy::clearProxy();
        }
        if (m_settings) {
            m_settings->setSystemProxyEnabled(false);
        } else {
            AppSettings::instance().setSystemProxyEnabled(false);
        }
    }
    if (m_settings) {
        m_settings->setTunEnabled(enabled);
    } else {
        AppSettings::instance().setTunEnabled(enabled);
    }
    return applySettingsToActiveConfig(true);
}

bool ProxyController::applySettingsToActiveConfig(bool restartIfRunning)
{
    QString configPath = activeConfigPath();
    if (configPath.isEmpty()) return false;

    ConfigRepository *repo = m_configRepo ? m_configRepo : ConfigProvider::instance();
    if (!repo) return false;
    QJsonObject config = repo->loadConfig(configPath);
    if (config.isEmpty()) return false;

    repo->applySettingsToConfig(config);
    repo->saveConfig(configPath, config);

    if (restartIfRunning && m_kernel && m_kernel->isRunning()) {
        m_kernel->restartWithConfig(configPath);
    }
    return true;
}

void ProxyController::updateSystemProxyForKernelState(bool running)
{
    const bool systemProxyEnabled = m_settings
                                        ? m_settings->systemProxyEnabled()
                                        : AppSettings::instance().systemProxyEnabled();
    if (!systemProxyEnabled) {
        if (m_systemProxy) {
            m_systemProxy->clearProxy();
        } else {
            SystemProxy::clearProxy();
        }
        return;
    }

    if (running) {
        ConfigRepository *repo = m_configRepo ? m_configRepo : ConfigProvider::instance();
        const int port = repo ? repo->mixedPort() : 1080;
        if (m_systemProxy) {
            m_systemProxy->setProxy("127.0.0.1", port);
        } else {
            SystemProxy::setProxy("127.0.0.1", port);
        }
    } else {
        if (m_systemProxy) {
            m_systemProxy->clearProxy();
        } else {
            SystemProxy::clearProxy();
        }
    }
}
