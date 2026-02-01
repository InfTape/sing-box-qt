#include "SystemProxyAdapter.h"

#include "system/SystemProxy.h"
void SystemProxyAdapter::setProxy(const QString& host, int port) {
  SystemProxy::setProxy(host, port);
}
void SystemProxyAdapter::clearProxy() { SystemProxy::clearProxy(); }
