#ifndef SYSTEMPROXYGATEWAY_H
#define SYSTEMPROXYGATEWAY_H
#include <QString>

class SystemProxyGateway {
 public:
  virtual ~SystemProxyGateway()                        = default;
  virtual bool setProxy(const QString& host, int port) = 0;
  virtual bool clearProxy()                            = 0;
};
#endif  // SYSTEMPROXYGATEWAY_H
