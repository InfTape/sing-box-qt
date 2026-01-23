#include "ProxyController.h"
#include "KernelService.h"
#include "storage/ConfigManager.h"
#include "storage/AppSettings.h"
#include "network/SubscriptionService.h"
#include "system/SystemProxy.h"
#include "utils/Logger.h"
#include <QFile>
#include <QObject>
#include <QJsonArray>
#include <QJsonObject>

ProxyController::ProxyController(KernelService *kernel,
                                 SubscriptionService *subscription,
                                 QObject *parent)
    : QObject(parent)
    , m_kernel(kernel)
    , m_subscription(subscription)
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
        configPath = ConfigManager::instance().getActiveConfigPath();
    }
    return configPath;
}

QString ProxyController::currentProxyMode() const
{
    const QString path = activeConfigPath();
    if (path.isEmpty()) return "rule";
    return ConfigManager::instance().readClashDefaultMode(path);
}

bool ProxyController::ensureConfigExists(QString *outPath)
{
    QString configPath = activeConfigPath();
    if (!configPath.isEmpty() && QFile::exists(configPath)) {
        if (outPath) *outPath = configPath;
        return true;
    }

    ConfigManager &cm = ConfigManager::instance();
    if (!cm.generateConfigWithNodes(QJsonArray())) {
        Logger::error(QStringLiteral(u"\u751f\u6210\u9ed8\u8ba4\u914d\u7f6e\u5931\u8d25"));
        return false;
    }

    configPath = cm.getActiveConfigPath();
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
        if (error) *error = QObject::tr("\u65e0\u6cd5\u83b7\u53d6\u914d\u7f6e\u6587\u4ef6\u8def\u5f84");
        return false;
    }

    bool ok = ConfigManager::instance().updateClashDefaultMode(configPath, mode, error);
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
    if (enabled) {
        int port = ConfigManager::instance().getMixedPort();
        SystemProxy::setProxy("127.0.0.1", port);
        AppSettings::instance().setSystemProxyEnabled(true);
        AppSettings::instance().setTunEnabled(false);
    } else {
        SystemProxy::clearProxy();
        AppSettings::instance().setSystemProxyEnabled(false);
    }
    return applySettingsToActiveConfig(true);
}

bool ProxyController::setTunModeEnabled(bool enabled)
{
    if (enabled) {
        SystemProxy::clearProxy();
        AppSettings::instance().setSystemProxyEnabled(false);
    }
    AppSettings::instance().setTunEnabled(enabled);
    return applySettingsToActiveConfig(true);
}

bool ProxyController::applySettingsToActiveConfig(bool restartIfRunning)
{
    QString configPath = activeConfigPath();
    if (configPath.isEmpty()) return false;

    QJsonObject config = ConfigManager::instance().loadConfig(configPath);
    if (config.isEmpty()) return false;

    ConfigManager::instance().applySettingsToConfig(config);
    ConfigManager::instance().saveConfig(configPath, config);

    if (restartIfRunning && m_kernel && m_kernel->isRunning()) {
        m_kernel->restartWithConfig(configPath);
    }
    return true;
}

void ProxyController::updateSystemProxyForKernelState(bool running)
{
    if (!AppSettings::instance().systemProxyEnabled()) {
        SystemProxy::clearProxy();
        return;
    }

    if (running) {
        int port = ConfigManager::instance().getMixedPort();
        SystemProxy::setProxy("127.0.0.1", port);
    } else {
        SystemProxy::clearProxy();
    }
}
