#ifndef PROXYVIEWCONTROLLER_H
#define PROXYVIEWCONTROLLER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <memory>

#include "core/DelayTestService.h"

class ProxyService;
class DelayTestService;
class ConfigRepository;
struct ProxyDelayTestResult;
/**
 * @brief ProxyViewController
 * 负责代理节点选择、延迟/测速等业务逻辑，避免 ProxyView 直接触碰核心服务。
 */
class ProxyViewController : public QObject {
  Q_OBJECT
 public:
  explicit ProxyViewController(ConfigRepository* configRepository,
                               QObject*          parent = nullptr);

  void          setProxyService(ProxyService* service);
  ProxyService* proxyService() const;

  bool isTesting() const;

  void refreshProxies() const;
  void selectProxy(const QString& group, const QString& proxy);

  void startSingleDelayTest(const QString& nodeName);
  void startBatchDelayTests(const QStringList& nodes);
  void stopAllTests();

  void startSpeedTest(const QString& nodeName, const QString& groupName);

  QJsonObject loadNodeOutbound(const QString& tag) const;

 signals:
  void proxiesUpdated(const QJsonObject& proxies);
  void proxySelected(const QString& group, const QString& proxy);
  void proxySelectFailed(const QString& group, const QString& proxy);

  void delayResult(const ProxyDelayTestResult& result);
  void testProgress(int current, int total);
  void testCompleted();

  void speedTestResult(const QString& nodeName, const QString& resultText);
  void errorOccurred(const QString& message);

 private:
  DelayTestService* ensureDelayTester();
  void              updateDelayTesterAuth();
  DelayTestOptions  buildSingleOptions() const;
  DelayTestOptions  buildBatchOptions() const;
  QString           runBandwidthTest(const QString& nodeTag) const;

  ProxyService*                     m_proxyService = nullptr;
  std::unique_ptr<DelayTestService> m_delayTestService;
  ConfigRepository*                 m_configRepository = nullptr;
};
#endif  // PROXYVIEWCONTROLLER_H
