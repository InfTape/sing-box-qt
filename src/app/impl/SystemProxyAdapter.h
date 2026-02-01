#ifndef SYSTEMPROXYADAPTER_H
#define SYSTEMPROXYADAPTER_H

#include "app/interfaces/SystemProxyGateway.h"
#include <QString>

class SystemProxyAdapter : public SystemProxyGateway
{
public:
    void setProxy(const QString &host, int port) override;
    void clearProxy() override;
};

#endif // SYSTEMPROXYADAPTER_H
