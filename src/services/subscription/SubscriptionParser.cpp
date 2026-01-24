#include "services/subscription/SubscriptionParser.h"
#include "utils/Logger.h"
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <yaml-cpp/yaml.h>

QJsonArray SubscriptionParser::parseSubscriptionContent(const QByteArray &content)
{
    QJsonDocument doc = QJsonDocument::fromJson(content);
    if (!doc.isNull() && doc.isObject()) {
        return parseSingBoxConfig(content);
    }

    const QString str = QString::fromUtf8(content);
    if (str.contains("proxies:")) {
        return parseClashConfig(content);
    }

    return parseURIList(content);
}

QJsonArray SubscriptionParser::parseSingBoxConfig(const QByteArray &content)
{
    QJsonDocument doc = QJsonDocument::fromJson(content);
    if (doc.isObject() && doc.object().contains("outbounds")) {
        return doc.object()["outbounds"].toArray();
    }
    return QJsonArray();
}

QJsonArray SubscriptionParser::parseClashConfig(const QByteArray &content)
{
    QJsonArray nodes;

    try {
        YAML::Node yaml = YAML::Load(content.toStdString());
        if (yaml["proxies"]) {
            for (const auto &proxy : yaml["proxies"]) {
                QJsonObject node;
                node["type"] = QString::fromStdString(proxy["type"].as<std::string>());
                node["tag"] = QString::fromStdString(proxy["name"].as<std::string>());
                node["server"] = QString::fromStdString(proxy["server"].as<std::string>());
                node["server_port"] = proxy["port"].as<int>();

                std::string type = proxy["type"].as<std::string>();
                if (type == "vmess") {
                    node["uuid"] = QString::fromStdString(proxy["uuid"].as<std::string>());
                    if (proxy["alterId"]) {
                        node["alter_id"] = proxy["alterId"].as<int>();
                    }
                } else if (type == "trojan") {
                    node["password"] = QString::fromStdString(proxy["password"].as<std::string>());
                } else if (type == "ss") {
                    node["type"] = "shadowsocks";
                    node["method"] = QString::fromStdString(proxy["cipher"].as<std::string>());
                    node["password"] = QString::fromStdString(proxy["password"].as<std::string>());
                }

                nodes.append(node);
            }
        }
    } catch (const std::exception &e) {
        Logger::error(QString("YAML parse error: %1").arg(e.what()));
    }

    return nodes;
}

QJsonArray SubscriptionParser::parseURIList(const QByteArray &content)
{
    QJsonArray nodes;

    QByteArray decoded = QByteArray::fromBase64(content);
    QString text = decoded.isEmpty()
        ? QString::fromUtf8(content)
        : QString::fromUtf8(decoded);

    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        const QString uri = line.trimmed();
        QJsonObject node;

        if (uri.startsWith("vmess://")) {
            node = parseVmessURI(uri);
        } else if (uri.startsWith("vless://")) {
            node = parseVlessURI(uri);
        } else if (uri.startsWith("trojan://")) {
            node = parseTrojanURI(uri);
        } else if (uri.startsWith("ss://")) {
            node = parseShadowsocksURI(uri);
        } else if (uri.startsWith("hysteria2://") || uri.startsWith("hy2://")) {
            node = parseHysteria2URI(uri);
        }

        if (!node.isEmpty()) {
            nodes.append(node);
        }
    }

    return nodes;
}

QJsonObject SubscriptionParser::parseVmessURI(const QString &uri)
{
    QJsonObject node;
    const QString encoded = uri.mid(8).trimmed();
    if (encoded.isEmpty()) {
        return node;
    }

    const QString decodedText = tryDecodeBase64ToText(encoded);
    if (decodedText.isEmpty()) {
        return node;
    }

    QJsonDocument doc = QJsonDocument::fromJson(decodedText.toUtf8());
    if (!doc.isObject()) {
        return node;
    }

    QJsonObject obj = doc.object();
    const QString server = obj.value("add").toString().trimmed();
    const QString uuid = obj.value("id").toString().trimmed();
    int port = obj.value("port").toVariant().toInt();
    if (port <= 0) {
        port = 443;
    }
    if (server.isEmpty() || uuid.isEmpty()) {
        return QJsonObject();
    }

    node["type"] = "vmess";
    node["server"] = server;
    node["server_port"] = port;
    node["uuid"] = uuid;

    QString tag = obj.value("ps").toString().trimmed();
    if (tag.isEmpty()) {
        tag = QString("vmess-%1:%2").arg(server, QString::number(port));
    }
    node["tag"] = tag;

    node["alter_id"] = obj.value("aid").toVariant().toInt();

    QString security = obj.value("scy").toString().trimmed();
    if (security.isEmpty()) {
        security = "auto";
    }
    node["security"] = security;

    const QString net = obj.value("net").toString().trimmed().toLower();
    const QString host = obj.value("host").toString().trimmed();
    const QString path = obj.value("path").toString().trimmed();
    const QString tls = obj.value("tls").toString().trimmed().toLower();
    const QString sni = obj.value("sni").toString().trimmed();
    const QString alpn = obj.value("alpn").toString().trimmed();
    const QString fp = obj.value("fp").toString().trimmed();

    bool tlsEnabled = (tls == "tls" || tls == "reality");
    bool insecure = false;
    if (obj.contains("allowInsecure")) {
        const QJsonValue insecureVal = obj.value("allowInsecure");
        insecure = insecureVal.isBool()
            ? insecureVal.toBool()
            : insecureVal.toString().trimmed() == "1";
    }
    if (tlsEnabled || !sni.isEmpty() || !alpn.isEmpty() || insecure || !fp.isEmpty()) {
        QJsonObject tlsObj;
        tlsObj["enabled"] = true;
        const QString serverName = !sni.isEmpty()
            ? sni
            : (!host.isEmpty() ? host : server);
        if (!serverName.isEmpty()) {
            tlsObj["server_name"] = serverName;
        }
        if (insecure) {
            tlsObj["insecure"] = true;
        }
        if (!alpn.isEmpty()) {
            QJsonArray alpnArr;
            const QStringList alpnList = alpn.split(",", Qt::SkipEmptyParts);
            for (const QString &item : alpnList) {
                const QString trimmed = item.trimmed();
                if (!trimmed.isEmpty()) {
                    alpnArr.append(trimmed);
                }
            }
            if (!alpnArr.isEmpty()) {
                tlsObj["alpn"] = alpnArr;
            }
        }

        QJsonObject utls;
        bool useUtls = false;
        if (!fp.isEmpty()) {
            utls["enabled"] = true;
            utls["fingerprint"] = fp;
            useUtls = true;
        } else if (tlsEnabled) {
            utls["enabled"] = true;
            utls["fingerprint"] = "chrome";
            useUtls = true;
        }
        if (useUtls) {
            tlsObj["utls"] = utls;
        }

        node["tls"] = tlsObj;
    }

    if (net == "ws") {
        QJsonObject transport;
        transport["type"] = "ws";
        if (!path.isEmpty()) {
            transport["path"] = path;
        }
        if (!host.isEmpty()) {
            QJsonObject headers;
            headers["Host"] = host;
            transport["headers"] = headers;
        }
        node["transport"] = transport;
    } else if (net == "grpc") {
        QJsonObject transport;
        transport["type"] = "grpc";
        QString serviceName = obj.value("serviceName").toString().trimmed();
        if (serviceName.isEmpty()) {
            serviceName = path;
        }
        if (!serviceName.isEmpty()) {
            transport["service_name"] = serviceName;
        }
        node["transport"] = transport;
    } else if (net == "h2" || net == "http") {
        QJsonObject transport;
        transport["type"] = "http";
        if (!path.isEmpty()) {
            transport["path"] = path;
        }
        if (!host.isEmpty()) {
            QJsonArray hostArr;
            const QStringList hostList = host.split(",", Qt::SkipEmptyParts);
            for (const QString &item : hostList) {
                const QString trimmed = item.trimmed();
                if (!trimmed.isEmpty()) {
                    hostArr.append(trimmed);
                }
            }
            if (!hostArr.isEmpty()) {
                transport["host"] = hostArr;
            }
        }
        node["transport"] = transport;
    }

    return node;
}

QJsonObject SubscriptionParser::parseVlessURI(const QString &uri)
{
    QJsonObject node;
    QUrl url(uri);
    QUrlQuery query(url.query());

    node["type"] = "vless";
    node["tag"] = QUrl::fromPercentEncoding(url.fragment().toUtf8());
    node["server"] = url.host();
    node["server_port"] = url.port();
    node["uuid"] = url.userName();

    QString flow = query.queryItemValue("flow");
    if (!flow.isEmpty()) {
        node["flow"] = flow;
    }

    QString security = query.queryItemValue("security");
    if (security == "tls" || security == "reality") {
        QJsonObject tls;
        tls["enabled"] = true;

        QString sni = query.queryItemValue("sni");
        if (!sni.isEmpty()) {
            tls["server_name"] = sni;
        }

        if (query.queryItemValue("allowInsecure") == "1") {
            tls["insecure"] = true;
        }

        QString alpn = query.queryItemValue("alpn");
        if (!alpn.isEmpty()) {
            tls["alpn"] = QJsonArray::fromStringList(alpn.split(","));
        }

        if (query.hasQueryItem("fp")) {
            QJsonObject utls;
            utls["enabled"] = true;
            utls["fingerprint"] = query.queryItemValue("fp");
            tls["utls"] = utls;
        }

        if (security == "reality") {
            QJsonObject reality;
            reality["enabled"] = true;
            reality["public_key"] = query.queryItemValue("pbk");
            QString shortId = query.queryItemValue("sid");
            if (!shortId.isEmpty()) {
                reality["short_id"] = shortId;
            }
            tls["reality"] = reality;
        }

        node["tls"] = tls;
    }

    node["packet_encoding"] = "xudp";

    QString type = query.queryItemValue("type");
    if (type == "ws") {
        QJsonObject transport;
        transport["type"] = "ws";

        QString path = query.queryItemValue("path");
        if (!path.isEmpty()) {
            transport["path"] = path;
        }

        QString host = query.queryItemValue("host");
        if (!host.isEmpty()) {
            QJsonObject headers;
            headers["Host"] = host;
            transport["headers"] = headers;
        }

        node["transport"] = transport;
    } else if (type == "grpc") {
        QJsonObject transport;
        transport["type"] = "grpc";

        QString serviceName = query.queryItemValue("serviceName");
        if (!serviceName.isEmpty()) {
            transport["service_name"] = serviceName;
        }

        node["transport"] = transport;
    } else if (type == "h2" || type == "http") {
        QJsonObject transport;
        transport["type"] = "http";

        QString path = query.queryItemValue("path");
        if (!path.isEmpty()) {
            transport["path"] = path;
        }

        QString host = query.queryItemValue("host");
        if (!host.isEmpty()) {
            transport["host"] = QJsonArray::fromStringList(host.split(","));
        }

        node["transport"] = transport;
    }

    return node;
}

QJsonObject SubscriptionParser::parseTrojanURI(const QString &uri)
{
    QJsonObject node;
    QUrl url(uri);
    QUrlQuery query(url.query());

    node["type"] = "trojan";
    node["tag"] = QUrl::fromPercentEncoding(url.fragment().toUtf8());
    node["server"] = url.host();
    node["server_port"] = url.port();
    node["password"] = url.userName();

    QJsonObject tls;
    tls["enabled"] = true;
    tls["server_name"] = query.queryItemValue("sni");
    tls["insecure"] = query.queryItemValue("allowInsecure") == "1";
    node["tls"] = tls;

    return node;
}

QJsonObject SubscriptionParser::parseShadowsocksURI(const QString &uri)
{
    QJsonObject node;
    QString data = uri.mid(5);

    int hashIndex = data.indexOf('#');
    QString tag;
    if (hashIndex != -1) {
        tag = QUrl::fromPercentEncoding(data.mid(hashIndex + 1).toUtf8());
        data = data.left(hashIndex);
    }

    QString userInfo;
    QString hostPart;

    if (data.contains("@")) {
        QStringList parts = data.split("@");
        QByteArray decoded = QByteArray::fromBase64(parts.first().toUtf8());
        if (decoded.contains(':')) {
            userInfo = QString::fromUtf8(decoded);
        } else {
            userInfo = parts.first();
        }
        hostPart = parts.last();
    } else {
        QByteArray decoded = QByteArray::fromBase64(data.toUtf8());
        QString full = QString::fromUtf8(decoded);
        int atIndex = full.lastIndexOf('@');
        if (atIndex != -1) {
            userInfo = full.left(atIndex);
            hostPart = full.mid(atIndex + 1);
        }
    }

    if (!userInfo.isEmpty()) {
        int colonIndex = userInfo.indexOf(':');
        if (colonIndex != -1) {
            node["type"] = "shadowsocks";
            node["tag"] = tag.isEmpty() ? hostPart : tag;
            node["method"] = userInfo.left(colonIndex);
            node["password"] = userInfo.mid(colonIndex + 1);

            int portIndex = hostPart.lastIndexOf(':');
            if (portIndex != -1) {
                node["server"] = hostPart.left(portIndex);
                node["server_port"] = hostPart.mid(portIndex + 1).toInt();
            }
        }
    }

    return node;
}

QJsonObject SubscriptionParser::parseHysteria2URI(const QString &uri)
{
    QJsonObject node;
    QString uriFixed = uri;
    if (uriFixed.startsWith("hy2://")) {
        uriFixed = "hysteria2://" + uriFixed.mid(6);
    }

    QUrl url(uriFixed);
    QUrlQuery query(url.query());

    node["type"] = "hysteria2";
    node["tag"] = QUrl::fromPercentEncoding(url.fragment().toUtf8());
    node["server"] = url.host();
    node["server_port"] = url.port();
    node["password"] = url.userName();

    if (node["password"].toString().isEmpty()) {
        node["password"] = query.queryItemValue("auth");
    }

    QJsonObject tls;
    tls["enabled"] = true;
    tls["server_name"] = query.queryItemValue("sni");
    tls["insecure"] = query.queryItemValue("insecure") == "1";
    node["tls"] = tls;

    if (query.hasQueryItem("obfs")) {
        QJsonObject obfs;
        obfs["type"] = query.queryItemValue("obfs");
        obfs["password"] = query.queryItemValue("obfs-password");
        node["obfs"] = obfs;
    }

    return node;
}

QString SubscriptionParser::tryDecodeBase64ToText(const QString &raw)
{
    QString compact = raw;
    compact.remove(QRegularExpression("\\s+"));
    if (compact.isEmpty()) {
        return QString();
    }

    int rem = compact.length() % 4;
    if (rem != 0) {
        compact.append(QString("=").repeated(4 - rem));
    }

    QByteArray input = compact.toUtf8();
    QByteArray decoded = QByteArray::fromBase64(input, QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded.isEmpty()) {
        return QString::fromUtf8(decoded);
    }

    decoded = QByteArray::fromBase64(input, QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded.isEmpty()) {
        return QString::fromUtf8(decoded);
    }

    return QString();
}

QJsonArray SubscriptionParser::extractNodesWithFallback(const QString &content)
{
    QJsonArray nodes = parseSubscriptionContent(content.toUtf8());
    if (!nodes.isEmpty()) {
        return nodes;
    }

    const QString decoded = tryDecodeBase64ToText(content);
    if (!decoded.isEmpty()) {
        nodes = parseSubscriptionContent(decoded.toUtf8());
        if (!nodes.isEmpty()) {
            return nodes;
        }
    }

    QString stripped = content.trimmed();
    stripped.replace("vmess://", "");
    stripped.replace("vless://", "");
    stripped.replace("trojan://", "");
    stripped.replace("ss://", "");
    stripped.replace("hysteria2://", "");
    stripped.replace("hy2://", "");
    const QString decodedStripped = tryDecodeBase64ToText(stripped);
    if (!decodedStripped.isEmpty()) {
        nodes = parseSubscriptionContent(decodedStripped.toUtf8());
    }
    return nodes;
}
