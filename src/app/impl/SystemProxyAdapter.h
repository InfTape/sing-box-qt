#ifndef SYSTEMPROXYADAPTER_H
#define SYSTEMPROXYADAPTER_H
#include <QString>
#include "app/interfaces/SystemProxyGateway.h"

class SystemProxyAdapter : public SystemProxyGateway {
 public:
  bool setProxy(const QString& host, int port) override;
  bool clearProxy() override;
};
#endif  // SYSTEMPROXYADAPTER_H
