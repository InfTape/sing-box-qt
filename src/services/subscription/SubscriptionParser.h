#ifndef SUBSCRIPTIONPARSER_H
#define SUBSCRIPTIONPARSER_H

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class SubscriptionParser
{
public:
    static QJsonArray parseSubscriptionContent(const QByteArray &content);
    static QJsonArray parseSingBoxConfig(const QByteArray &content);
    static QJsonArray parseClashConfig(const QByteArray &content);
    static QJsonArray parseURIList(const QByteArray &content);

    static QJsonObject parseVmessURI(const QString &uri);
    static QJsonObject parseVlessURI(const QString &uri);
    static QJsonObject parseTrojanURI(const QString &uri);
    static QJsonObject parseShadowsocksURI(const QString &uri);
    static QJsonObject parseHysteria2URI(const QString &uri);

    static QJsonArray extractNodesWithFallback(const QString &content);
    static QString tryDecodeBase64ToText(const QString &raw);
};

#endif // SUBSCRIPTIONPARSER_H
