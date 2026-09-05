#include "ProxyRuntimeController.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QTimer>
#include "core/KernelService.h"
#include "core/ProxyController.h"
#include "core/ProxyService.h"

ProxyRuntimeController::ProxyRuntimeController(KernelService*   kernelService,
                                               ProxyService*    proxyService,
                                               ProxyController* proxyController,
                                               QObject*         parent)
    : QObject(parent),
      m_kernelService(kernelService),
      m_proxyService(proxyService),
      m_proxyController(proxyController) {
  if (m_kernelService) {
    connect(m_kernelService,
            &KernelService::statusChanged,
            this,
            &ProxyRuntimeController::onKernelStatusChanged);
    connect(m_kernelService,
            &KernelService::dataUsageUpdated,
            this,
            &ProxyRuntimeController::dataUsageUpdated);
    connect(m_kernelService,
            &KernelService::apiLogReceived,
            this,
            &ProxyRuntimeController::apiLogMessage);
    connect(m_kernelService,
            &KernelService::errorOccurred,
            this,
            [this](const QString& error) {
              emit apiLogMessage("error",
                                 QString("[Kernel Error] %1").arg(error));
            });
  }
  if (m_proxyService) {
    connect(m_proxyService,
            &ProxyService::trafficUpdated,
            this,
            &ProxyRuntimeController::trafficUpdated);
    connect(m_proxyService,
            &ProxyService::connectionsReceived,
            this,
            &ProxyRuntimeController::handleConnectionsJson);
  }
  m_connectionsTimer = new QTimer(this);
  m_connectionsTimer->setInterval(2000);
  connect(m_connectionsTimer, &QTimer::timeout, this, [this]() {
    if (m_proxyService && isKernelRunning()) {
      m_proxyService->fetchConnections();
    }
  });
}

bool ProxyRuntimeController::isKernelRunning() const {
  return m_kernelService && m_kernelService->isRunning();
}

void ProxyRuntimeController::broadcastStates() {
  onKernelStatusChanged(isKernelRunning());
  if (m_kernelService) {
    m_kernelService->requestDataUsage();
  }
}

void ProxyRuntimeController::setLogsStreamingActive(bool active) {
  m_logsPageActive = active;
}

void ProxyRuntimeController::onKernelStatusChanged(bool running) {
  emit kernelRunningChanged(running);
  if (m_proxyService) {
    if (running) {
      m_proxyService->startTrafficMonitor();
    } else {
      m_proxyService->stopTrafficMonitor();
    }
  }
  if (m_connectionsTimer) {
    if (running) {
      if (!m_connectionsTimer->isActive()) {
        m_connectionsTimer->start();
      }
      if (m_proxyService) {
        m_proxyService->fetchConnections();
      }
    } else {
      m_connectionsTimer->stop();
    }
  }
  if (running && m_kernelService) {
    m_kernelService->requestDataUsage();
  }
  if (m_proxyController) {
    m_proxyController->updateSystemProxyForKernelState(running);
  }
  if (running) {
    QTimer::singleShot(
        1000, this, &ProxyRuntimeController::refreshProxyViewRequested);
    QTimer::singleShot(
        1200, this, &ProxyRuntimeController::refreshRulesViewRequested);
    QTimer::singleShot(
        20000, this, &ProxyRuntimeController::refreshProxyViewRequested);
  }
}

void ProxyRuntimeController::handleConnectionsJson(
    const QJsonObject& connections) {
  const QJsonArray conns       = connections.value("connections").toArray();
  QJsonValue       memoryValue = connections.value("memory");
  if (memoryValue.isUndefined()) {
    memoryValue = connections.value("memory_usage");
  }
  if (memoryValue.isUndefined()) {
    memoryValue = connections.value("memoryUsage");
  }
  const qint64 memoryUsage = memoryValue.toVariant().toLongLong();
  emit         connectionsUpdated(conns.count(), memoryUsage);
}

void ProxyRuntimeController::clearDataUsage() {
  if (m_kernelService) {
    m_kernelService->resetDataUsage();
  }
}


