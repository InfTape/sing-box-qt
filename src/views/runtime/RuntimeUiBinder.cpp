#include "views/runtime/RuntimeUiBinder.h"
#include <QObject>
#include <QPushButton>
#include <QStyle>
#include "app/ProxyRuntimeController.h"
#include "views/connections/ConnectionsView.h"
#include "views/home/HomeView.h"
#include "views/logs/LogView.h"
#include "views/proxy/ProxyView.h"
#include "views/rules/RulesView.h"

RuntimeUiBinder::RuntimeUiBinder(ProxyRuntimeController* runtime, HomeView* home, ConnectionsView* connections,
                                 ProxyView* proxy, RulesView* rules, LogView* log, QPushButton* startStopBtn)
    : m_runtime(runtime),
      m_home(home),
      m_connections(connections),
      m_proxy(proxy),
      m_rules(rules),
      m_log(log),
      m_startStopBtn(startStopBtn) {}

void RuntimeUiBinder::bind() {
  if (!m_runtime) return;
  // Kernel running state → UI (status, auto refresh, button text)
  if (m_home) {
    QObject::connect(m_runtime, &ProxyRuntimeController::kernelRunningChanged, m_home, &HomeView::updateStatus);
  }
  if (m_connections) {
    QObject::connect(m_runtime, &ProxyRuntimeController::kernelRunningChanged, m_connections,
                     &ConnectionsView::setAutoRefreshEnabled);
  }
  if (m_startStopBtn) {
    QObject::connect(m_runtime, &ProxyRuntimeController::kernelRunningChanged, m_startStopBtn,
                     [btn = m_startStopBtn](bool running) {
                       btn->setText(running ? QObject::tr("Stop") : QObject::tr("Start"));
                       btn->setProperty("state", running ? "stop" : "start");
                       btn->style()->polish(btn);
                     });
  }
  // Traffic / connections metrics → home view
  if (m_home) {
    QObject::connect(m_runtime, &ProxyRuntimeController::trafficUpdated, m_home, &HomeView::updateTraffic);
    QObject::connect(m_runtime, &ProxyRuntimeController::connectionsUpdated, m_home,
                     [this](int count, qint64 memoryUsage) {
                       if (m_home) m_home->updateConnections(count, memoryUsage);
                     });
    QObject::connect(m_runtime, &ProxyRuntimeController::dataUsageUpdated, m_home, &HomeView::updateDataUsage);
    QObject::connect(m_home, &HomeView::dataUsageClearRequested, m_runtime, &ProxyRuntimeController::clearDataUsage);
  }
  // Refresh requests → corresponding views
  if (m_proxy) {
    QObject::connect(m_runtime, &ProxyRuntimeController::refreshProxyViewRequested, m_proxy, &ProxyView::refresh);
  }
  if (m_rules) {
    QObject::connect(m_runtime, &ProxyRuntimeController::refreshRulesViewRequested, m_rules, &RulesView::refresh);
  }
  // Logs → log view
  if (m_log) {
    QObject::connect(m_runtime, &ProxyRuntimeController::logMessage, m_log, [this](const QString& msg, bool) {
      if (m_log) m_log->appendLog(msg);
    });
  }
  // Initial broadcast
  m_runtime->broadcastStates();
}
