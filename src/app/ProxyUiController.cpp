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
  if (!m_proxyController->toggleKernel()) {
    if (error) {
      *error =
          tr("Config file not found at the expected location; startup failed.");
    }
    return false;
  }
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
    bool enabled, const std::function<bool()>& confirmRestartAdmin) {
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
  const bool isAdmin = m_adminActions ? m_adminActions->isAdmin() : false;
  if (!isAdmin) {
    if (confirmRestartAdmin && confirmRestartAdmin()) {
      const bool proxyDisabled = disableSystemProxyIfNeeded();
      if (!proxyDisabled) {
        restoreSystemProxyIfNeeded();
        emitCurrentStates();
        return TunResult::Failed;
      }
      const bool tunPrepared = m_proxyController->setTunModeEnabled(true, false);
      if (!tunPrepared) {
        restoreSystemProxyIfNeeded();
        emitCurrentStates();
        return TunResult::Failed;
      }
      const bool restarted =
          m_adminActions ? m_adminActions->restartAsAdmin() : false;
      if (!restarted) {
        m_proxyController->setTunModeEnabled(false, false);
        restoreSystemProxyIfNeeded();
        emitCurrentStates();
        return TunResult::Failed;
      }
      emitCurrentStates();
      return TunResult::Applied;
    }
    m_settings->setTunEnabled(false);
    emitCurrentStates();
    return TunResult::Cancelled;
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
  if (m_proxyController) {
    m_proxyController->updateSystemProxyForKernelState(false);
  }
  if (m_kernelService && m_kernelService->isRunning()) {
    m_kernelService->stop();
  }
}

void ProxyUiController::broadcastCurrentStates() {
  if (m_settings) {
    const bool isAdmin = m_adminActions ? m_adminActions->isAdmin() : false;
    if (!isAdmin && m_settings->tunEnabled()) {
      m_settings->setTunEnabled(false);
      if (m_proxyController) {
        m_proxyController->setSystemProxyEnabled(true);
      } else {
        m_settings->setSystemProxyEnabled(true);
      }
    }
    emit systemProxyStateChanged(m_settings->systemProxyEnabled());
    emit tunModeStateChanged(m_settings->tunEnabled());
  }
  emit proxyModeChanged(currentProxyMode());
}
