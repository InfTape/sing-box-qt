#ifndef PROXYRUNTIMECONTROLLER_H
#define PROXYRUNTIMECONTROLLER_H

#include <QObject>

class KernelService;
class ProxyService;
class ProxyController;
class QJsonObject;
/**
 * @brief ProxyRuntimeController
 * 监听 Kernel/Proxy 服务运行态，将状态、流量、连接等信号分发给 UI，
 * 同时在内核启动/停止时负责与业务层的配合（系统代理同步、流量监控启停）。
 */
class ProxyRuntimeController : public QObject {
  Q_OBJECT
 public:
  explicit ProxyRuntimeController(KernelService*   kernelService,
                                  ProxyService*    proxyService,
                                  ProxyController* proxyController,
                                  QObject*         parent = nullptr);

  bool isKernelRunning() const;
  void broadcastStates();

 signals:
  void kernelRunningChanged(bool running);
  void trafficUpdated(qint64 upload, qint64 download);
  void connectionsUpdated(int count, qint64 memoryUsage);
  void logMessage(const QString& message, bool isError);
  void refreshProxyViewRequested();
  void refreshRulesViewRequested();

 private slots:
  void onKernelStatusChanged(bool running);
  void handleConnectionsJson(const QJsonObject& connections);

 private:
  KernelService*   m_kernelService   = nullptr;
  ProxyService*    m_proxyService    = nullptr;
  ProxyController* m_proxyController = nullptr;
};
#endif  // PROXYRUNTIMECONTROLLER_H
