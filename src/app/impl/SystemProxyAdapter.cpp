#include "SystemProxyAdapter.h"
#include "system/SystemProxy.h"

bool SystemProxyAdapter::setProxy(const QString& host, int port) {
  return SystemProxy::setProxy(host, port);
}

bool SystemProxyAdapter::clearProxy() {
  return SystemProxy::clearProxy();
}
