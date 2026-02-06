#include "utils/proxy/ProxyActions.h"
#include "core/DelayTestService.h"
#include "core/ProxyService.h"

namespace ProxyActions {
bool selectProxy(ProxyService* service, const QString& group, const QString& node) {
  if (!service || group.isEmpty() || node.isEmpty()) return false;
  service->selectProxy(group, node);
  return true;
}

bool startNodeDelayTest(DelayTestService* service, const QString& node, const DelayTestOptions& options) {
  if (!service || node.isEmpty()) return false;
  service->testNodeDelay(node, options);
  return true;
}

bool startNodesDelayTests(DelayTestService* service, const QStringList& nodes, const DelayTestOptions& options) {
  if (!service || nodes.isEmpty()) return false;
  service->testNodesDelay(nodes, options);
  return true;
}

void stopAllTests(DelayTestService* service) {
  if (!service) return;
  service->stopAllTests();
}
}  // namespace ProxyActions
