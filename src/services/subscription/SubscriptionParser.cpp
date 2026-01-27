#include "services/subscription/SubscriptionParser.h"
#include "utils/Logger.h"
#include <QJsonDocument>
#include <QJsonParseError>
#include <QList>
#include <QMap>
#include <QSet>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <yaml-cpp/yaml.h>

namespace {
void appendArray(QJsonArray &target, const QJsonArray &source)
{
    for (const auto &val : source) {
        target.append(val);
    }
}

bool looksLikeWireguardConfig(const QString &text)
{
    return text.contains("[Interface]") && text.contains("[Peer]");
}

QString normalizeHostToAscii(const QString &host)
{
    if (host.isEmpty()) {
        return host;
    }
    const QByteArray ace = QUrl::toAce(host);
    if (!ace.isEmpty()) {
        return QString::fromLatin1(ace);
    }
    return host;
}

QList<QString> splitContentEntries(const QString &text)
{
    QList<QString> entries;
    int idx = 0;
    const int len = text.length();
    while (idx < len) {
        const QChar ch = text[idx];
        if (ch == '\n' || ch == '\r') {
            idx += 1;
            continue;
        }
        if (ch == '{' || ch == '[') {
            const QChar open = ch;
            const QChar close = (ch == '{') ? '}' : ']';
            int depth = 1;
            int i = idx + 1;
            for (; i < len; ++i) {
                if (text[i] == open) depth++;
                else if (text[i] == close) {
                    depth--;
                    if (depth == 0) break;
                }
            }
            const int end = (i < len) ? i + 1 : len;
            entries.append(text.mid(idx, end - idx).trimmed());
            idx = end;
            continue;
        }
        int nl = text.indexOf('\n', idx);
        if (nl == -1) nl = len;
        const QString segment = text.mid(idx, nl - idx).trimmed();
        if (!segment.isEmpty()) {
            entries.append(segment);
        }
        idx = nl + 1;
    }
    return entries;
}

QJsonArray parseJsonContentToNodes(const QByteArray &content)
{
    QJsonArray result;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(content, &err);
    if (err.error != QJsonParseError::NoError) {
        return result;
    }

    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("outbounds") || obj.contains("endpoints")) {
            return SubscriptionParser::parseSingBoxConfig(content);
        }
        if (obj.contains("servers")) {
            return SubscriptionParser::parseSip008Config(obj);
        }
        QJsonObject single = SubscriptionParser::parseSingleJsonNode(obj);
        if (!single.isEmpty()) {
            result.append(single);
        }
        return result;
    }

    if (doc.isArray()) {
        for (const auto &item : doc.array()) {
            if (!item.isObject()) continue;
            QJsonObject single = SubscriptionParser::parseSingleJsonNode(item.toObject());
            if (!single.isEmpty()) {
                result.append(single);
            }
        }
    }
    return result;
}
} // namespace

QJsonArray SubscriptionParser::parseSubscriptionContent(const QByteArray &content)
{
    QJsonArray jsonNodes = parseJsonContentToNodes(content);
    if (!jsonNodes.isEmpty()) {
        return jsonNodes;
    }

    const QString str = QString::fromUtf8(content);
    if (looksLikeWireguardConfig(str)) {
        QJsonObject wg = parseWireguardConfig(str);
        if (!wg.isEmpty()) {
            QJsonArray arr;
            arr.append(wg);
            return arr;
        }
    }

    if (str.contains("proxies")) {
        return parseClashConfig(content);
    }

    return parseURIList(content);
}

QJsonArray SubscriptionParser::parseSingBoxConfig(const QByteArray &content)
{
    QJsonArray nodes;
    QJsonDocument doc = QJsonDocument::fromJson(content);
    if (!doc.isObject()) {
        return nodes;
    }

    auto isProxyOutbound = [](const QJsonObject &ob) -> bool {
        const QString type = ob.value("type").toString().trimmed().toLower();
        if (type.isEmpty()) return false;

        static const QSet<QString> proxyTypes = {
            "socks", "http", "shadowsocks", "vmess", "vless", "trojan",
            "anytls", "hysteria", "hysteria2", "tuic", "wireguard", "ssh"
        };
        if (!proxyTypes.contains(type)) {
            return false;
        }

        const QString server = ob.value("server").toString().trimmed();
        int port = ob.value("server_port").toVariant().toInt();
        if (port <= 0) {
            port = ob.value("port").toVariant().toInt();
        }
        return !server.isEmpty() && port > 0;
    };

    QJsonObject root = doc.object();
    const QJsonArray outbounds = root.value("outbounds").toArray();
    for (const auto &ob : outbounds) {
        if (!ob.isObject()) continue;
        QJsonObject outbound = ob.toObject();
        if (!isProxyOutbound(outbound)) continue;
        nodes.append(outbound);
    }

    const QJsonArray endpoints = root.value("endpoints").toArray();
    for (const auto &ep : endpoints) {
        if (!ep.isObject()) continue;
        QJsonObject endpoint = ep.toObject();
        if (!isProxyOutbound(endpoint)) continue;
        nodes.append(endpoint);
    }

    return nodes;
}

QJsonArray SubscriptionParser::parseClashConfig(const QByteArray &content)
{
    QJsonArray nodes;

    auto readString = [](const YAML::Node &node) -> QString {
        if (!node || !node.IsScalar()) {
            return QString();
        }
        return QString::fromStdString(node.as<std::string>());
    };

    auto readInt = [](const YAML::Node &node, int defaultValue = 0) -> int {
        if (!node || !node.IsScalar()) {
            return defaultValue;
        }
        bool ok = false;
        const int value = QString::fromStdString(node.as<std::string>()).toInt(&ok);
        return ok ? value : defaultValue;
    };

    auto readBool = [](const YAML::Node &node, bool defaultValue = false) -> bool {
        if (!node || !node.IsScalar()) {
            return defaultValue;
        }
        const QString text = QString::fromStdString(node.as<std::string>()).trimmed().toLower();
        if (text == "true" || text == "1") {
            return true;
        }
        if (text == "false" || text == "0") {
            return false;
        }
        return defaultValue;
    };

    auto readStringList = [&](const YAML::Node &node) -> QJsonArray {
        QJsonArray list;
        if (!node) {
            return list;
        }
        if (node.IsSequence()) {
            for (const auto &item : node) {
                const QString value = readString(item).trimmed();
                if (!value.isEmpty()) {
                    list.append(value);
                }
            }
        } else {
            const QString value = readString(node).trimmed();
            if (!value.isEmpty()) {
                list.append(value);
            }
        }
        return list;
    };

    try {
        YAML::Node yaml = YAML::Load(content.toStdString());
        if (yaml["proxies"]) {
            for (const auto &proxy : yaml["proxies"]) {
                QJsonObject node;
                const QString type = readString(proxy["type"]).trimmed().toLower();
                if (type.isEmpty()) {
                    continue;
                }

                node["type"] = (type == "ss") ? "shadowsocks" : type;
                node["tag"] = readString(proxy["name"]);
                node["server"] = readString(proxy["server"]);
                node["server_port"] = readInt(proxy["port"], 0);

                if (type == "vmess") {
                    node["uuid"] = readString(proxy["uuid"]);
                    node["alter_id"] = readInt(proxy["alterId"], 0);
                    const QString security = readString(proxy["cipher"]).trimmed();
                    node["security"] = security.isEmpty() ? "auto" : security;
                } else if (type == "vless") {
                    node["uuid"] = readString(proxy["uuid"]);
                    const QString flow = readString(proxy["flow"]).trimmed();
                    if (!flow.isEmpty()) {
                        node["flow"] = flow;
                    }
                } else if (type == "trojan") {
                    node["password"] = readString(proxy["password"]);
                } else if (type == "ss") {
                    node["method"] = readString(proxy["cipher"]);
                    node["password"] = readString(proxy["password"]);
                }

                const bool supportsTransport = (type == "vmess" || type == "vless" || type == "trojan");
                if (supportsTransport) {
                    const bool tlsEnabled = readBool(proxy["tls"], false);
                    const QString serverName = readString(proxy["servername"]).trimmed();
                    const QString sni = readString(proxy["sni"]).trimmed();
                    const QString peer = readString(proxy["peer"]).trimmed();
                    const bool insecure = readBool(proxy["skip-cert-verify"], false)
                        || readBool(proxy["allowInsecure"], false);

                    if (tlsEnabled || !serverName.isEmpty() || !sni.isEmpty() || !peer.isEmpty() || insecure) {
                        QJsonObject tlsObj;
                        tlsObj["enabled"] = true;

                        QString tlsServerName = serverName;
                        if (tlsServerName.isEmpty()) {
                            tlsServerName = sni;
                        }
                        if (tlsServerName.isEmpty()) {
                            tlsServerName = peer;
                        }
                        if (tlsServerName.isEmpty()) {
                            tlsServerName = node.value("server").toString();
                        }
                        if (!tlsServerName.isEmpty()) {
                            tlsObj["server_name"] = tlsServerName;
                        }
                        if (insecure) {
                            tlsObj["insecure"] = true;
                        }
                        if (tlsEnabled && type == "vmess") {
                            QJsonObject utls;
                            utls["enabled"] = true;
                            utls["fingerprint"] = "chrome";
                            tlsObj["utls"] = utls;
                        }
                        node["tls"] = tlsObj;
                    }

                    const QString network = readString(proxy["network"]).trimmed().toLower();
                    if (network == "ws") {
                        QJsonObject transport;
                        transport["type"] = "ws";

                        YAML::Node wsOpts = proxy["ws-opts"];
                        QString path;
                        if (wsOpts && wsOpts.IsMap()) {
                            path = readString(wsOpts["path"]).trimmed();
                        }
                        if (path.isEmpty()) {
                            path = readString(proxy["path"]).trimmed();
                        }
                        if (!path.isEmpty()) {
                            transport["path"] = path;
                        }

                        QJsonObject headersObj;
                        if (wsOpts && wsOpts.IsMap()) {
                            YAML::Node headers = wsOpts["headers"];
                            if (headers && headers.IsMap()) {
                                for (auto it = headers.begin(); it != headers.end(); ++it) {
                                    const QString key = readString(it->first).trimmed();
                                    const QString value = readString(it->second).trimmed();
                                    if (!key.isEmpty() && !value.isEmpty()) {
                                        headersObj[key] = value;
                                    }
                                }
                            }
                        }
                        if (!headersObj.isEmpty()) {
                            transport["headers"] = headersObj;
                        }

                        node["transport"] = transport;
                    } else if (network == "grpc") {
                        QJsonObject transport;
                        transport["type"] = "grpc";

                        YAML::Node grpcOpts = proxy["grpc-opts"];
                        QString serviceName;
                        if (grpcOpts && grpcOpts.IsMap()) {
                            serviceName = readString(grpcOpts["grpc-service-name"]).trimmed();
                        }
                        if (serviceName.isEmpty()) {
                            serviceName = readString(proxy["grpc-service-name"]).trimmed();
                        }
                        if (serviceName.isEmpty()) {
                            serviceName = readString(proxy["path"]).trimmed();
                        }
                        if (!serviceName.isEmpty()) {
                            transport["service_name"] = serviceName;
                        }

                        node["transport"] = transport;
                    } else if (network == "h2" || network == "http") {
                        QJsonObject transport;
                        transport["type"] = "http";

                        YAML::Node opts = (network == "h2") ? proxy["h2-opts"] : proxy["http-opts"];
                        QString path;
                        if (opts && opts.IsMap()) {
                            path = readString(opts["path"]).trimmed();
                        }
                        if (!path.isEmpty()) {
                            transport["path"] = path;
                        }

                        QJsonArray hostList;
                        if (opts && opts.IsMap()) {
                            hostList = readStringList(opts["host"]);
                        }
                        if (!hostList.isEmpty()) {
                            transport["host"] = hostList;
                        }

                        node["transport"] = transport;
                    }
                }

                nodes.append(node);
            }
        }
    } catch (const std::exception &e) {
        Logger::error(QString("YAML parse error: %1").arg(e.what()));
    }

    return nodes;
}

QJsonArray SubscriptionParser::parseSip008Config(const QJsonObject &obj)
{
    QJsonArray nodes;
    if (!obj.contains("servers") || !obj.value("servers").isArray()) {
        return nodes;
    }

    const QJsonArray servers = obj.value("servers").toArray();
    for (const auto &serverVal : servers) {
        if (!serverVal.isObject()) continue;
        const QJsonObject serverObj = serverVal.toObject();
        QJsonObject node;
        node["type"] = "shadowsocks";
        node["server"] = serverObj.value("server").toString();
        node["server_port"] = serverObj.value("server_port").toVariant().toInt();
        node["method"] = serverObj.value("method").toString();
        node["password"] = serverObj.value("password").toString();
        QString tag = serverObj.value("remarks").toString().trimmed();
        if (tag.isEmpty()) {
            tag = serverObj.value("name").toString().trimmed();
        }
        if (tag.isEmpty()) {
            tag = QString("%1:%2").arg(node.value("server").toString())
                .arg(node.value("server_port").toInt());
        }
        node["tag"] = tag;
        if (!node.value("server").toString().isEmpty() && node.value("server_port").toInt() > 0) {
            nodes.append(node);
        }
    }

    return nodes;
}

QJsonObject SubscriptionParser::parseSingleJsonNode(const QJsonObject &obj)
{
    QJsonObject node = obj;

    QString type = node.value("type").toString().trimmed();
    if (type.isEmpty()) {
        type = node.value("protocol").toString().trimmed();
    }

    QString server = node.value("server").toString().trimmed();
    if (server.isEmpty()) {
        server = node.value("address").toString().trimmed();
    }
    if (server.isEmpty()) {
        server = node.value("host").toString().trimmed();
    }

    int port = node.value("server_port").toVariant().toInt();
    if (port <= 0) {
        port = node.value("port").toVariant().toInt();
    }

    QString tag = node.value("tag").toString().trimmed();
    if (tag.isEmpty()) {
        tag = node.value("name").toString().trimmed();
    }
    if (tag.isEmpty() && !server.isEmpty() && port > 0) {
        tag = QString("%1:%2").arg(server).arg(port);
    }

    if (type.isEmpty() || server.isEmpty() || port <= 0) {
        return QJsonObject();
    }

    node["type"] = type;
    node["server"] = server;
    node["server_port"] = port;
    if (!tag.isEmpty()) {
        node["tag"] = tag;
    }
    return node;
}

QJsonArray SubscriptionParser::parseURIList(const QByteArray &content)
{
    QJsonArray nodes;

    QString text = tryDecodeBase64ToText(QString::fromUtf8(content));
    if (text.isEmpty()) {
        text = QString::fromUtf8(content);
    }

    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        const QString uri = line.trimmed();
        QJsonObject node;

        if (uri.startsWith("{") || uri.startsWith("[")) {
            appendArray(nodes, parseJsonContentToNodes(uri.toUtf8()));
            continue;
        } else if (uri.startsWith("json://")) {
            const QString decoded = tryDecodeBase64ToText(uri.mid(7));
            if (!decoded.isEmpty()) {
                appendArray(nodes, parseJsonContentToNodes(decoded.toUtf8()));
                continue;
            }
        } else if (uri.startsWith("vmess://")) {
            node = parseVmessURI(uri);
        } else if (uri.startsWith("vless://")) {
            node = parseVlessURI(uri);
        } else if (uri.startsWith("trojan://")) {
            node = parseTrojanURI(uri);
        } else if (uri.startsWith("ss://")) {
            node = parseShadowsocksURI(uri);
        } else if (uri.startsWith("hysteria2://") || uri.startsWith("hy2://")) {
            node = parseHysteria2URI(uri);
        } else if (uri.startsWith("hysteria://")) {
            node = parseHysteriaURI(uri);
        } else if (uri.startsWith("tuic://")) {
            node = parseTuicURI(uri);
        } else if (uri.startsWith("socks://") || uri.startsWith("socks5://") || uri.startsWith("socks4://") || uri.startsWith("socks4a://")) {
            node = parseSocksURI(uri);
        } else if (uri.startsWith("http://") || uri.startsWith("https://")) {
            node = parseHttpURI(uri);
        } else if (uri.startsWith("wg://")) {
            node = parseWireguardConfig(uri);
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
    node["server"] = normalizeHostToAscii(url.host());
    int port = url.port();
    if (port <= 0) {
        port = 443;
    }
    node["server_port"] = port;
    node["uuid"] = url.userName();

    QString tag = QUrl::fromPercentEncoding(url.fragment().toUtf8());
    if (tag.isEmpty()) {
        tag = QString("vless-%1:%2").arg(node["server"].toString()).arg(port);
    }
    node["tag"] = tag;

    QString flow = query.queryItemValue("flow");
    if (!flow.isEmpty()) {
        node["flow"] = flow;
    }

    QString security = query.queryItemValue("security");
    if (security == "tls" || security == "reality") {
        QJsonObject tls;
        tls["enabled"] = true;

        QString sni = normalizeHostToAscii(query.queryItemValue("sni"));
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

    // 仅当订阅显式指定 packetEncoding/pe 时才开启，避免对不支持 XUDP 的节点造成问题
    QString packetEncoding = query.queryItemValue("packetEncoding");
    if (packetEncoding.isEmpty()) {
        packetEncoding = query.queryItemValue("packetencoding");
    }
    if (packetEncoding.isEmpty()) {
        packetEncoding = query.queryItemValue("pe");
    }
    if (!packetEncoding.isEmpty()) {
        node["packet_encoding"] = packetEncoding;
    }

    QString type = query.queryItemValue("type");
    if (type == "ws") {
        QJsonObject transport;
        transport["type"] = "ws";

        QString path = query.queryItemValue("path");
        if (!path.isEmpty()) {
            path = QUrl::fromPercentEncoding(path.toUtf8());
        }
        if (!path.isEmpty()) {
            transport["path"] = path;
        }

        QString host = normalizeHostToAscii(query.queryItemValue("host"));
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
            path = QUrl::fromPercentEncoding(path.toUtf8());
        }
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
    node["server"] = url.host();
    int port = url.port();
    if (port <= 0) {
        port = 443;
    }
    node["server_port"] = port;
    node["password"] = url.userName();

    QString tag = QUrl::fromPercentEncoding(url.fragment().toUtf8());
    if (tag.isEmpty()) {
        tag = QString("trojan-%1:%2").arg(node["server"].toString()).arg(port);
    }
    node["tag"] = tag;

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
    node["server"] = url.host();
    int port = url.port();
    if (port <= 0) {
        port = 443;
    }
    node["server_port"] = port;
    node["password"] = url.userName();

    if (node["password"].toString().isEmpty()) {
        node["password"] = query.queryItemValue("auth");
    }

    QString tag = QUrl::fromPercentEncoding(url.fragment().toUtf8());
    if (tag.isEmpty()) {
        tag = QString("hy2-%1:%2").arg(node.value("server").toString()).arg(port);
    }
    node["tag"] = tag;

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

QJsonObject SubscriptionParser::parseHysteriaURI(const QString &uri)
{
    QJsonObject node;
    QUrl url(uri);
    if (!url.isValid()) {
        return node;
    }
    QUrlQuery query(url.query());

    const QString host = url.host();
    int port = url.port();
    if (port <= 0) {
        port = 443;
    }

    node["type"] = "hysteria";
    node["server"] = host;
    node["server_port"] = port;

    QString tag = QUrl::fromPercentEncoding(url.fragment().toUtf8());
    if (tag.isEmpty()) {
        tag = QString("hysteria-%1:%2").arg(host).arg(port);
    }
    node["tag"] = tag;

    QString auth = url.userName();
    if (auth.isEmpty()) {
        auth = query.queryItemValue("auth");
    }
    if (!auth.isEmpty()) {
        node["auth"] = auth;
    }

    QString up = query.queryItemValue("up");
    QString down = query.queryItemValue("down");
    if (!up.isEmpty()) node["up_mbps"] = up;
    if (!down.isEmpty()) node["down_mbps"] = down;

    QJsonObject tls;
    tls["enabled"] = true;
    QString sni = query.queryItemValue("sni");
    QString peer = query.queryItemValue("peer");
    if (!sni.isEmpty() || !peer.isEmpty()) {
        const QString serverName = !sni.isEmpty() ? sni : peer;
        tls["server_name"] = serverName;
    }
    if (query.queryItemValue("insecure") == "1" || query.queryItemValue("allow_insecure") == "1") {
        tls["insecure"] = true;
    }
    if (!tls.isEmpty()) {
        node["tls"] = tls;
    }

    const QString obfsType = query.queryItemValue("obfs");
    QString obfsParam = query.queryItemValue("obfs-password");
    if (obfsParam.isEmpty()) {
        obfsParam = query.queryItemValue("obfsParam");
    }
    if (!obfsType.isEmpty()) {
        QJsonObject obfs;
        obfs["type"] = obfsType;
        if (!obfsParam.isEmpty()) {
            obfs["password"] = obfsParam;
        }
        node["obfs"] = obfs;
    }

    return node;
}

QJsonObject SubscriptionParser::parseTuicURI(const QString &uri)
{
    QJsonObject node;
    QUrl url(uri);
    if (!url.isValid()) {
        return node;
    }

    QUrlQuery query(url.query());
    const QString host = url.host();
    int port = url.port();
    if (port <= 0) {
        port = 443;
    }

    node["type"] = "tuic";
    node["server"] = host;
    node["server_port"] = port;

    QString tag = QUrl::fromPercentEncoding(url.fragment().toUtf8());
    if (tag.isEmpty()) {
        tag = QString("tuic-%1:%2").arg(host).arg(port);
    }
    node["tag"] = tag;

    const QString uuid = url.userName();
    const QString password = url.password();
    if (!uuid.isEmpty()) node["uuid"] = uuid;
    if (!password.isEmpty()) node["password"] = password;

    const QString token = query.queryItemValue("token");
    if (!token.isEmpty()) node["token"] = token;

    const QString congestion = query.queryItemValue("congestion_control").isEmpty()
        ? query.queryItemValue("congestion")
        : query.queryItemValue("congestion_control");
    if (!congestion.isEmpty()) {
        node["congestion_control"] = congestion;
    }

    const QString udpRelay = query.queryItemValue("udp_relay_mode");
    if (!udpRelay.isEmpty()) {
        node["udp_relay_mode"] = udpRelay;
    }

    const QString heartbeat = query.queryItemValue("heartbeat_interval");
    if (!heartbeat.isEmpty()) {
        node["heartbeat"] = heartbeat;
    }

    QString alpn = query.queryItemValue("alpn");
    if (!alpn.isEmpty()) {
        node["alpn"] = QJsonArray::fromStringList(alpn.split(",", Qt::SkipEmptyParts));
    }

    const bool insecure = query.queryItemValue("allow_insecure") == "1" || query.queryItemValue("insecure") == "1";
    QString sni = query.queryItemValue("sni");
    QString peer = query.queryItemValue("peer");
    if (peer.isEmpty()) {
        peer = query.queryItemValue("serverName");
    }
    if (insecure || !sni.isEmpty() || !peer.isEmpty()) {
        QJsonObject tls;
        tls["enabled"] = true;
        QString serverName = !sni.isEmpty() ? sni : peer;
        if (serverName.isEmpty()) {
            serverName = host;
        }
        if (!serverName.isEmpty()) {
            tls["server_name"] = serverName;
        }
        if (insecure) {
            tls["insecure"] = true;
        }
        node["tls"] = tls;
    }

    return node;
}

QJsonObject SubscriptionParser::parseSocksURI(const QString &uri)
{
    QJsonObject node;
    QUrl url(uri);
    if (!url.isValid()) {
        return node;
    }

    const QString host = url.host();
    int port = url.port();
    if (host.isEmpty() || port <= 0) {
        return node;
    }

    node["type"] = "socks";
    node["server"] = host;
    node["server_port"] = port;

    QString tag = QUrl::fromPercentEncoding(url.fragment().toUtf8());
    if (tag.isEmpty()) {
        tag = QString("socks-%1:%2").arg(host).arg(port);
    }
    node["tag"] = tag;

    if (!url.userName().isEmpty()) {
        node["username"] = url.userName();
    }
    if (!url.password().isEmpty()) {
        node["password"] = url.password();
    }
    return node;
}

QJsonObject SubscriptionParser::parseHttpURI(const QString &uri)
{
    QJsonObject node;
    QUrl url(uri);
    if (!url.isValid()) {
        return node;
    }

    const QString host = url.host();
    int port = url.port();
    if (host.isEmpty()) {
        return node;
    }
    if (port <= 0) {
        port = url.scheme().toLower() == "https" ? 443 : 80;
    }

    node["type"] = "http";
    node["server"] = host;
    node["server_port"] = port;

    QString tag = QUrl::fromPercentEncoding(url.fragment().toUtf8());
    if (tag.isEmpty()) {
        tag = QString("http-%1:%2").arg(host).arg(port);
    }
    node["tag"] = tag;

    if (!url.userName().isEmpty()) {
        node["username"] = url.userName();
    }
    if (!url.password().isEmpty()) {
        node["password"] = url.password();
    }

    if (url.scheme().toLower() == "https") {
        QJsonObject tls;
        tls["enabled"] = true;
        tls["server_name"] = host;
        node["tls"] = tls;
    }

    return node;
}

QJsonObject SubscriptionParser::parseWireguardConfig(const QString &content)
{
    if (content.startsWith("wg://")) {
        const QString encoded = content.mid(5);
        const QString decoded = tryDecodeBase64ToText(encoded);
        if (!decoded.isEmpty() && decoded != content) {
            return parseWireguardConfig(decoded);
        }
    }

    QMap<QString, QString> interfaceMap;
    QMap<QString, QString> peerMap;
    QMap<QString, QString> *current = nullptr;

    const QStringList lines = content.split('\n');
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';')) {
            continue;
        }
        if (line.startsWith('[') && line.endsWith(']')) {
            const QString section = line.mid(1, line.length() - 2).toLower();
            if (section == "interface") {
                current = &interfaceMap;
            } else if (section == "peer") {
                current = &peerMap;
            } else {
                current = nullptr;
            }
            continue;
        }

        const int eqIndex = line.indexOf('=');
        if (eqIndex <= 0 || !current) {
            continue;
        }
        const QString key = line.left(eqIndex).trimmed().toLower();
        const QString value = line.mid(eqIndex + 1).trimmed();
        (*current)[key] = value;
    }

    const QString privateKey = interfaceMap.value("privatekey");
    const QString endpoint = peerMap.value("endpoint");
    if (privateKey.isEmpty() || endpoint.isEmpty()) {
        return QJsonObject();
    }

    QString host;
    int port = 0;
    int colon = endpoint.lastIndexOf(':');
    if (colon > 0) {
        host = endpoint.left(colon).trimmed();
        port = endpoint.mid(colon + 1).toInt();
    } else {
        host = endpoint.trimmed();
    }
    if (port <= 0) {
        port = 51820;
    }

    QStringList addresses;
    const QString addrValue = interfaceMap.value("address");
    for (const QString &addr : addrValue.split(',', Qt::SkipEmptyParts)) {
        const QString trimmed = addr.trimmed();
        if (!trimmed.isEmpty()) {
            addresses.append(trimmed);
        }
    }

    QJsonObject node;
    node["type"] = "wireguard";
    node["server"] = host;
    node["server_port"] = port;
    node["private_key"] = privateKey;
    const QString peerPublic = peerMap.value("publickey");
    if (!peerPublic.isEmpty()) {
        node["peer_public_key"] = peerPublic;
    }
    const QString preShared = peerMap.value("presharedkey");
    if (!preShared.isEmpty()) {
        node["pre_shared_key"] = preShared;
    }
    if (!addresses.isEmpty()) {
        node["local_address"] = QJsonArray::fromStringList(addresses);
    }

    QString tag = interfaceMap.value("description").trimmed();
    if (tag.isEmpty()) {
        tag = QString("wireguard-%1:%2").arg(host).arg(port);
    }
    node["tag"] = tag;

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
    auto tryParse = [](const QString &text) -> QJsonArray {
        QJsonArray parsed = SubscriptionParser::parseSubscriptionContent(text.toUtf8());
        if (!parsed.isEmpty()) {
            return parsed;
        }

        QJsonArray merged;
        const QList<QString> parts = splitContentEntries(text);
        for (const auto &part : parts) {
            QJsonArray sub = SubscriptionParser::parseSubscriptionContent(part.toUtf8());
            appendArray(merged, sub);
        }
        return merged;
    };

    QJsonArray nodes = tryParse(content.trimmed());
    if (!nodes.isEmpty()) {
        return nodes;
    }

    const QString decoded = tryDecodeBase64ToText(content);
    if (!decoded.isEmpty()) {
        nodes = tryParse(decoded);
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
    stripped.replace("hysteria://", "");
    stripped.replace("tuic://", "");
    stripped.replace("wg://", "");
    const QString decodedStripped = tryDecodeBase64ToText(stripped);
    if (!decodedStripped.isEmpty()) {
        nodes = tryParse(decodedStripped);
    }
    return nodes;
}
