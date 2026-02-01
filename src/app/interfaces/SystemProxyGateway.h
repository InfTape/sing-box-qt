#ifndef SYSTEMPROXYGATEWAY_H
#define SYSTEMPROXYGATEWAY_H

#include <QString>

class SystemProxyGateway
{
public:
    virtual ~SystemProxyGateway() = default;
    virtual void setProxy(const QString &host, int port) = 0;
    virtual void clearProxy() = 0;
};

#endif // SYSTEMPROXYGATEWAY_H
