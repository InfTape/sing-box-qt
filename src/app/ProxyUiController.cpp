#include "ProxyUiController.h"
#include "app/interfaces/AdminActions.h"
#include "app/interfaces/SettingsStore.h"
#include "core/KernelService.h"
#include "core/ProxyController.h"

ProxyUiController::ProxyUiController(ProxyController* proxyController,
                                     KernelService*   kernelService,
                                     SettingsStore*   settingsStore,
                                     AdminActions*    adminActions,
                                     QObject*         parent)
    : QObject(parent),
      m_proxyController(proxyController),
      m_kernelService(kernelService),
      m_settings(settingsStore),
      m_adminActions(adminActions) {}

bool ProxyUiController::isKernelRunning() const {
  return m_kernelService && m_kernelService->isRunning();
}

bool ProxyUiController::isKernelInstalled() const {
  return m_kernelService && !m_kernelService->getKernelPath().isEmpty();
}

QString ProxyUiController::currentProxyMode() const {
  return m_proxyController ? m_proxyController->currentProxyMode()
                           : QStringLiteral("rule");
}

bool ProxyUiController::systemProxyEnabled() const {
  return m_settings && m_settings->systemProxyEnabled();
}

bool ProxyUiController::tunModeEnabled() const {
  return m_settings && m_settings->tunEnabled();
}

bool ProxyUiController::toggleKernel(QString* error) {
  if (!m_proxyController) {
    if (error) {
      *error = tr("Proxy controller is unavailable.");
    }
    return false;
  }
  const bool wasRunning = m_kernelService && m_kernelService->isRunning();
  if (wasRunning) {
    if (m_settings) {
      m_settings->setKernelAutoStartEnabled(true);
    }
    return true;
  }
  if (!m_proxyController->startKernel()) {
    if (error) {
      *error =
          tr("Config file not found at the expected location; startup failed.");
    }
    return false;
  }
  if (m_settings) {
    m_settings->setKernelAutoStartEnabled(true);
  }
  return true;
}

bool ProxyUiController::restoreStartupRuntime(
    const std::function<bool()>& confirmRestartAdmin, QString* error) {
  if (!m_proxyController || !m_settings) {
    if (error) {
      *error = tr("Proxy components are unavailable.");
    }
    return false;
  }
  m_settings->setKernelAutoStartEnabled(true);
  if (!isKernelInstalled()) {
    if (error) {
      *error = tr("Kernel is not installed yet. Please download it first.");
    }
    return false;
  }
  if (m_settings->tunEnabled()) {
    if (m_settings->systemProxyEnabled()) {
      if (!m_proxyController->setSystemProxyEnabled(false)) {
        if (error) {
          *error = tr("Failed to disable system proxy for TUN mode.");
        }
        return false;
      }
    }
  } else if (m_settings->systemProxyEnabled()) {
    if (!m_proxyController->setSystemProxyEnabled(true)) {
      if (error) {
        *error = tr("Failed to restore system proxy settings.");
      }
      return false;
    }
  }
  if (m_kernelService && m_kernelService->isRunning()) {
    emit systemProxyStateChanged(m_settings->systemProxyEnabled());
    emit tunModeStateChanged(m_settings->tunEnabled());
    return true;
  }
  if (!m_proxyController->syncSettingsToActiveConfig(false)) {
    if (error) {
      *error =
          tr("Config file not found at the expected location; startup failed.");
    }
    return false;
  }
  if (!m_proxyController->startKernel()) {
    const bool needsTunElevation =
        m_settings->tunEnabled() &&
        !(m_adminActions && m_adminActions->isAdmin());
    if (needsTunElevation && confirmRestartAdmin && confirmRestartAdmin()) {
      const bool restarted =
          m_adminActions ? m_adminActions->restartAsAdmin() : false;
      if (!restarted && error) {
        *error = tr("Failed to restart as administrator.");
      }
      return restarted;
    }
    if (error) {
      *error =
          tr("Config file not found at the expected location; startup failed.");
    }
    return false;
  }
  m_settings->setKernelAutoStartEnabled(true);
  emit systemProxyStateChanged(m_settings->systemProxyEnabled());
  emit tunModeStateChanged(m_settings->tunEnabled());
  return true;
}

bool ProxyUiController::switchProxyMode(const QString& mode, QString* error) {
  if (!m_proxyController) {
    if (error) {
      *error = tr("Proxy controller is unavailable.");
    }
    return false;
  }
  const bool restartKernel = m_kernelService && m_kernelService->isRunning();
  if (!m_proxyController->setProxyMode(mode, restartKernel, error)) {
    return false;
  }
  emit proxyModeChanged(mode);
  return true;
}

bool ProxyUiController::setSystemProxyEnabled(bool enabled, QString* error) {
  if (!m_proxyController || !m_settings) {
    if (error) {
      *error = tr("Proxy components are unavailable.");
    }
    return false;
  }
  if (!m_proxyController->setSystemProxyEnabled(enabled)) {
    if (error) {
      *error = tr("Failed to update system proxy settings.");
    }
    return false;
  }
  emit systemProxyStateChanged(m_settings->systemProxyEnabled());
  emit tunModeStateChanged(m_settings->tunEnabled());
  return true;
}

ProxyUiController::TunResult ProxyUiController::setTunModeEnabled(
    bool enabled) {
  if (!m_proxyController || !m_settings) {
    return TunResult::Failed;
  }
  const bool previousSystemProxyEnabled = m_settings->systemProxyEnabled();
  const auto emitCurrentStates          = [this]() {
    emit tunModeStateChanged(m_settings->tunEnabled());
    emit systemProxyStateChanged(m_settings->systemProxyEnabled());
  };
  const auto disableSystemProxyIfNeeded = [this]() {
    if (!m_settings->systemProxyEnabled()) {
      return true;
    }
    return m_proxyController->setSystemProxyEnabled(false);
  };
  const auto restoreSystemProxyIfNeeded = [this, previousSystemProxyEnabled]() {
    if (!previousSystemProxyEnabled || m_settings->systemProxyEnabled()) {
      return;
    }
    m_proxyController->setSystemProxyEnabled(true);
  };
  if (!enabled) {
    const bool ok = m_proxyController->setTunModeEnabled(false);
    emitCurrentStates();
    return ok ? TunResult::Applied : TunResult::Failed;
  }
  const bool proxyDisabled = disableSystemProxyIfNeeded();
  if (!proxyDisabled) {
    restoreSystemProxyIfNeeded();
    emitCurrentStates();
    return TunResult::Failed;
  }
  const bool ok = m_proxyController->setTunModeEnabled(true);
  if (!ok) {
    restoreSystemProxyIfNeeded();
  }
  emitCurrentStates();
  return ok ? TunResult::Applied : TunResult::Failed;
}

void ProxyUiController::prepareForExit() {
  if (m_settings) {
    m_settings->setKernelAutoStartEnabled(true);
  }
}

void ProxyUiController::broadcastCurrentStates() {
  if (m_settings) {
    emit systemProxyStateChanged(m_settings->systemProxyEnabled());
    emit tunModeStateChanged(m_settings->tunEnabled());
  }
  emit proxyModeChanged(currentProxyMode());
}
