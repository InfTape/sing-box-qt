#include "ProxyRuntimeController.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QTimer>
#include "core/KernelService.h"
#include "core/ProxyController.h"
#include "core/ProxyService.h"
#include "core/DataUsageTracker.h"

ProxyRuntimeController::ProxyRuntimeController(KernelService*   kernelService,
                                               ProxyService*    proxyService,
                                               ProxyController* proxyController,
                                               QObject*         parent)
    : QObject(parent),
      m_kernelService(kernelService),
      m_proxyService(proxyService),
      m_proxyController(proxyController),
      m_dataUsageTracker(new DataUsageTracker(this)),
      m_connectionsTimer(new QTimer(this)) {
  if (m_connectionsTimer) {
    m_connectionsTimer->setInterval(2000);
    connect(m_connectionsTimer, &QTimer::timeout, this, [this]() {
      if (m_proxyService && isKernelRunning()) {
        m_proxyService->fetchConnections();
      }
    });
  }
  if (m_dataUsageTracker) {
    connect(m_dataUsageTracker,
            &DataUsageTracker::dataUsageUpdated,
            this,
            &ProxyRuntimeController::dataUsageUpdated);
  }
  if (m_kernelService) {
    connect(m_kernelService,
            &KernelService::statusChanged,
            this,
            &ProxyRuntimeController::onKernelStatusChanged);
    connect(m_kernelService,
            &KernelService::outputReceived,
            this,
            [this](const QString& line) { emit logMessage(line, false); });
    connect(m_kernelService,
            &KernelService::errorOccurred,
            this,
            [this](const QString& error) {
              emit logMessage(QString("[ERROR] %1").arg(error), true);
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
}

bool ProxyRuntimeController::isKernelRunning() const {
  return m_kernelService && m_kernelService->isRunning();
}

void ProxyRuntimeController::broadcastStates() {
  onKernelStatusChanged(isKernelRunning());
  if (m_dataUsageTracker) {
    emit dataUsageUpdated(m_dataUsageTracker->snapshot());
  }
}

void ProxyRuntimeController::onKernelStatusChanged(bool running) {
  emit kernelRunningChanged(running);
  if (m_proxyService) {
    if (running) {
      m_proxyService->startTrafficMonitor();
      if (m_connectionsTimer && !m_connectionsTimer->isActive()) {
        m_connectionsTimer->start();
        m_proxyService->fetchConnections();
      }
    } else {
      m_proxyService->stopTrafficMonitor();
      if (m_connectionsTimer)
        m_connectionsTimer->stop();
    }
  } else if (!running) {
    if (m_connectionsTimer)
      m_connectionsTimer->stop();
  }
  if (!running && m_dataUsageTracker) {
    m_dataUsageTracker->resetSession();
  }
  if (m_proxyController) {
    m_proxyController->updateSystemProxyForKernelState(running);
  }
  if (running) {
    QTimer::singleShot(
        1000, this, &ProxyRuntimeController::refreshProxyViewRequested);
    QTimer::singleShot(
        1200, this, &ProxyRuntimeController::refreshRulesViewRequested);
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
  if (m_dataUsageTracker) {
    m_dataUsageTracker->updateFromConnections(connections);
  }
}

void ProxyRuntimeController::clearDataUsage() {
  if (m_dataUsageTracker) {
    m_dataUsageTracker->reset();
  }
}
