#ifndef CONFIGBUILDER_H
#define CONFIGBUILDER_H

#include <QJsonArray>
#include <QJsonObject>

class ConfigBuilder
{
public:
    static QJsonObject buildBaseConfig();
    static QJsonObject buildDnsConfig();
    static QJsonObject buildRouteConfig();
    static QJsonArray buildInbounds();
    static QJsonArray buildOutbounds();
    static QJsonArray buildRuleSets();
    static QJsonObject buildExperimental();
};

#endif // CONFIGBUILDER_H
