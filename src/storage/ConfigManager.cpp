#include "ConfigManager.h"
#include "AppSettings.h"
#include "ConfigConstants.h"
#include "utils/AppPaths.h"
#include "utils/Logger.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QHostAddress>
#include <QSet>
#include <limits>

namespace {
bool isIpAddress(const QString &value)
{
    QHostAddress addr;
    return addr.setAddress(value);
}

QJsonObject makeRemoteRuleSet(const QString &tag,
                              const QString &url,
                              const QString &downloadDetour,
                              const QString &updateInterval)
{
    QJsonObject rs;
    rs["tag"] = tag;
    rs["type"] = "remote";
    rs["format"] = "binary";
    rs["url"] = url;
    rs["download_detour"] = downloadDetour;
    rs["update_interval"] = updateInterval;
    return rs;
}
} // namespace

ConfigManager& ConfigManager::instance()
{
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_mixedPort(AppSettings::instance().mixedPort())
    , m_apiPort(AppSettings::instance().apiPort())
{
}

QString ConfigManager::getConfigDir() const
{
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString dataDir = appDataDir();
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    // 迁移旧配置到新目录
    const QString newConfig = QDir(dataDir).filePath("config.json");
    const QString oldConfig1 = QDir(baseDir).filePath("sing-box-qt/config.json");
    const QString oldConfig2 = QDir(baseDir).filePath("config.json");
    if (!QFile::exists(newConfig)) {
        if (QFile::exists(oldConfig1)) {
            QFile::copy(oldConfig1, newConfig);
        } else if (QFile::exists(oldConfig2)) {
            QFile::copy(oldConfig2, newConfig);
        }
    }
    return dataDir;
}

QString ConfigManager::getActiveConfigPath() const
{
    return getConfigDir() + "/config.json";
}

int ConfigManager::getMixedPort() const
{
    return AppSettings::instance().mixedPort();
}

int ConfigManager::getApiPort() const
{
    return AppSettings::instance().apiPort();
}

void ConfigManager::setMixedPort(int port)
{
    m_mixedPort = port;
    AppSettings::instance().setMixedPort(port);
}

void ConfigManager::setApiPort(int port)
{
    m_apiPort = port;
    AppSettings::instance().setApiPort(port);
}

QJsonObject ConfigManager::loadConfig(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::warn(QString("无法打开配置文件: %1").arg(path));
        return QJsonObject();
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    return doc.object();
}

bool ConfigManager::saveConfig(const QString &path, const QJsonObject &config)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        Logger::error(QString("无法写入配置文件: %1").arg(path));
        return false;
    }

    QJsonDocument doc(config);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    Logger::info(QString("配置已保存: %1").arg(path));
    return true;
}

QJsonObject ConfigManager::generateBaseConfig()
{
    QJsonObject config;

    QJsonObject log;
    log["disabled"] = false;
    log["level"] = "info";
    log["timestamp"] = true;
    config["log"] = log;

    config["dns"] = buildDnsConfig();
    config["inbounds"] = buildInbounds();
    config["outbounds"] = buildOutboundGroups();
    config["route"] = buildRouteConfig();
    config["experimental"] = buildExperimental();

    applySettingsToConfig(config);

    return config;
}

bool ConfigManager::generateConfigWithNodes(const QJsonArray &nodes, const QString &targetPath)
{
    QJsonObject config = generateBaseConfig();
    if (!injectNodes(config, nodes)) {
        return false;
    }

    const QString path = targetPath.isEmpty() ? getActiveConfigPath() : targetPath;
    return saveConfig(path, config);
}

bool ConfigManager::injectNodes(QJsonObject &config, const QJsonArray &nodes)
{
    QJsonArray outbounds = config.value("outbounds").toArray();

    QSet<QString> existingTags;
    for (const auto &obVal : outbounds) {
        if (!obVal.isObject()) continue;
        const QJsonObject ob = obVal.toObject();
        const QString tag = ob.value("tag").toString().trimmed();
        if (!tag.isEmpty()) {
            existingTags.insert(tag);
        }
    }

    QStringList groupNodeTags;
    QJsonArray normalizedNodes;
    const QString resolverStrategy = AppSettings::instance().dnsStrategy();

    for (int i = 0; i < nodes.size(); ++i) {
        if (!nodes[i].isObject()) {
            Logger::error(QString("节点不是对象: index=%1").arg(i));
            return false;
        }
        QJsonObject node = nodes[i].toObject();

        const QString rawTag = node.value("tag").toString().trimmed();
        if (rawTag.isEmpty()) {
            Logger::error(QString("节点缺少 tag: index=%1").arg(i));
            return false;
        }
        if (node.value("type").toString().trimmed().isEmpty()) {
            Logger::error(QString("节点缺少 type: tag=%1, index=%2").arg(rawTag).arg(i));
            return false;
        }

        QString tag = rawTag;
        if (existingTags.contains(tag)) {
            QString candidate = QString("节点-%1-%2").arg(rawTag).arg(i);
            if (!existingTags.contains(candidate)) {
                tag = candidate;
            } else {
                int counter = 1;
                while (true) {
                    candidate = QString("节点-%1-%2").arg(rawTag).arg(counter);
                    if (!existingTags.contains(candidate)) {
                        tag = candidate;
                        break;
                    }
                    if (counter < std::numeric_limits<int>::max()) {
                        counter += 1;
                    }
                }
            }
        }
        existingTags.insert(tag);
        node["tag"] = tag;

        const QString server = node.value("server").toString().trimmed();
        if (!server.isEmpty()
            && server != "0.0.0.0"
            && !isIpAddress(server)
            && !node.contains("domain_resolver")) {
            QJsonObject resolver;
            resolver["server"] = ConfigConstants::DNS_RESOLVER;
            resolver["strategy"] = resolverStrategy;
            node["domain_resolver"] = resolver;
        }

        if (shouldIncludeNodeInGroups(node)) {
            groupNodeTags.append(tag);
        }
        normalizedNodes.append(node);
    }

    updateUrltestAndSelector(outbounds, groupNodeTags);
    updateAppGroupSelectors(outbounds, groupNodeTags);

    for (const auto &nodeVal : normalizedNodes) {
        outbounds.append(nodeVal);
    }

    config["outbounds"] = outbounds;
    return true;
}

void ConfigManager::applySettingsToConfig(QJsonObject &config)
{
    const AppSettings &settings = AppSettings::instance();
    m_mixedPort = settings.mixedPort();
    m_apiPort = settings.apiPort();

    config["inbounds"] = buildInbounds();

    QJsonObject experimental = config.value("experimental").toObject();
    QJsonObject clashApi = experimental.value("clash_api").toObject();
    clashApi["external_controller"] = QString("127.0.0.1:%1").arg(settings.apiPort());
    clashApi["external_ui_download_detour"] = settings.normalizedDownloadDetour();
    experimental["clash_api"] = clashApi;
    config["experimental"] = experimental;

    QJsonObject dns = config.value("dns").toObject();
    dns["strategy"] = settings.dnsStrategy();

    if (dns.contains("servers") && dns["servers"].isArray()) {
        QJsonArray servers = dns.value("servers").toArray();
        for (int i = 0; i < servers.size(); ++i) {
            if (!servers[i].isObject()) continue;
            QJsonObject server = servers[i].toObject();
            const QString tag = server.value("tag").toString();
            if (tag == ConfigConstants::DNS_PROXY) {
                server["address"] = settings.dnsProxy();
                server["detour"] = settings.normalizedDefaultOutbound();
            } else if (tag == ConfigConstants::DNS_CN) {
                server["address"] = settings.dnsCn();
            } else if (tag == ConfigConstants::DNS_RESOLVER) {
                server["address"] = settings.dnsResolver();
            }
            servers[i] = server;
        }
        dns["servers"] = servers;
    }

    if (dns.contains("rules") && dns["rules"].isArray()) {
        QJsonArray rules = dns.value("rules").toArray();
        int adsIndex = -1;
        for (int i = 0; i < rules.size(); ++i) {
            if (!rules[i].isObject()) continue;
            const QJsonObject rule = rules[i].toObject();
            if (rule.value("rule_set").toString() == ConfigConstants::RS_GEOSITE_ADS) {
                adsIndex = i;
                break;
            }
        }

        if (settings.blockAds()) {
            if (adsIndex < 0) {
                QJsonObject adsRule;
                adsRule["rule_set"] = ConfigConstants::RS_GEOSITE_ADS;
                adsRule["server"] = ConfigConstants::DNS_BLOCK;
                rules.insert(0, adsRule);
            } else {
                QJsonObject rule = rules[adsIndex].toObject();
                rule["server"] = ConfigConstants::DNS_BLOCK;
                rules[adsIndex] = rule;
            }
        } else if (adsIndex >= 0) {
            rules.removeAt(adsIndex);
        }

        dns["rules"] = rules;
    }

    config["dns"] = dns;

    if (config.contains("outbounds") && config["outbounds"].isArray()) {
        QJsonArray outbounds = config.value("outbounds").toArray();
        for (int i = 0; i < outbounds.size(); ++i) {
            if (!outbounds[i].isObject()) continue;
            QJsonObject ob = outbounds[i].toObject();
            if (ob.value("tag").toString() == ConfigConstants::TAG_AUTO) {
                ob["interrupt_exist_connections"] = true;
                ob["idle_timeout"] = "10m";
                ob["url"] = settings.urltestUrl();
                outbounds[i] = ob;
            }
        }

        if (!settings.enableAppGroups()) {
            QJsonArray filtered;
            for (const auto &obVal : outbounds) {
                if (!obVal.isObject()) {
                    filtered.append(obVal);
                    continue;
                }
                const QString tag = obVal.toObject().value("tag").toString();
                if (tag != ConfigConstants::TAG_TELEGRAM
                    && tag != ConfigConstants::TAG_YOUTUBE
                    && tag != ConfigConstants::TAG_NETFLIX
                    && tag != ConfigConstants::TAG_OPENAI) {
                    filtered.append(obVal);
                }
            }
            outbounds = filtered;
        }

        config["outbounds"] = outbounds;
    }

    if (config.contains("route") && config["route"].isObject()) {
        QJsonObject route = config.value("route").toObject();
        route["final"] = settings.normalizedDefaultOutbound();
        route["default_domain_resolver"] = ConfigConstants::DNS_RESOLVER;

        if (route.contains("rule_set") && route["rule_set"].isArray()) {
            QJsonArray ruleSets = route.value("rule_set").toArray();
            for (int i = 0; i < ruleSets.size(); ++i) {
                if (!ruleSets[i].isObject()) continue;
                QJsonObject rs = ruleSets[i].toObject();
                if (rs.value("type").toString() == "remote") {
                    rs["download_detour"] = settings.normalizedDownloadDetour();
                }
                ruleSets[i] = rs;
            }

            if (!settings.blockAds()) {
                QJsonArray filtered;
                for (const auto &rsVal : ruleSets) {
                    if (!rsVal.isObject()) {
                        filtered.append(rsVal);
                        continue;
                    }
                    if (rsVal.toObject().value("tag").toString() != ConfigConstants::RS_GEOSITE_ADS) {
                        filtered.append(rsVal);
                    }
                }
                ruleSets = filtered;
            }

            if (!settings.enableAppGroups()) {
                QJsonArray filtered;
                for (const auto &rsVal : ruleSets) {
                    if (!rsVal.isObject()) {
                        filtered.append(rsVal);
                        continue;
                    }
                    const QString tag = rsVal.toObject().value("tag").toString();
                    if (tag != ConfigConstants::RS_GEOSITE_TELEGRAM
                        && tag != ConfigConstants::RS_GEOSITE_YOUTUBE
                        && tag != ConfigConstants::RS_GEOSITE_NETFLIX
                        && tag != ConfigConstants::RS_GEOSITE_OPENAI) {
                        filtered.append(rsVal);
                    }
                }
                ruleSets = filtered;
            }

            route["rule_set"] = ruleSets;
        }

        if (route.contains("rules") && route["rules"].isArray()) {
            QJsonArray rules = route.value("rules").toArray();

            for (int i = 0; i < rules.size(); ++i) {
                if (!rules[i].isObject()) continue;
                QJsonObject rule = rules[i].toObject();
                if (rule.value("clash_mode").toString() == "global") {
                    rule["outbound"] = settings.normalizedDefaultOutbound();
                }
                if (rule.value("rule_set").toString() == ConfigConstants::RS_GEOSITE_GEOLOCATION_NOT_CN) {
                    rule["outbound"] = settings.normalizedDefaultOutbound();
                }
                rules[i] = rule;
            }

            int hijackIndex = -1;
            for (int i = 0; i < rules.size(); ++i) {
                if (!rules[i].isObject()) continue;
                const QJsonObject rule = rules[i].toObject();
                if (rule.value("protocol").toString() == "dns"
                    && rule.value("action").toString() == "hijack-dns") {
                    hijackIndex = i;
                    break;
                }
            }
            if (settings.dnsHijack()) {
                if (hijackIndex < 0) {
                    QJsonObject hijackRule;
                    hijackRule["protocol"] = "dns";
                    hijackRule["action"] = "hijack-dns";
                    rules.insert(1, hijackRule);
                }
            } else if (hijackIndex >= 0) {
                rules.removeAt(hijackIndex);
            }

            int adsIndex = -1;
            for (int i = 0; i < rules.size(); ++i) {
                if (!rules[i].isObject()) continue;
                const QJsonObject rule = rules[i].toObject();
                if (rule.value("rule_set").toString() == ConfigConstants::RS_GEOSITE_ADS
                    && rule.contains("action")) {
                    adsIndex = i;
                    break;
                }
            }
            if (settings.blockAds()) {
                if (adsIndex < 0) {
                    QJsonObject adsRule;
                    adsRule["rule_set"] = ConfigConstants::RS_GEOSITE_ADS;
                    adsRule["action"] = "reject";
                    rules.append(adsRule);
                }
            } else if (adsIndex >= 0) {
                rules.removeAt(adsIndex);
            }

            if (!settings.enableAppGroups()) {
                QJsonArray filtered;
                for (const auto &ruleVal : rules) {
                    if (!ruleVal.isObject()) {
                        filtered.append(ruleVal);
                        continue;
                    }
                    const QString rs = ruleVal.toObject().value("rule_set").toString();
                    if (rs != ConfigConstants::RS_GEOSITE_TELEGRAM
                        && rs != ConfigConstants::RS_GEOSITE_YOUTUBE
                        && rs != ConfigConstants::RS_GEOSITE_NETFLIX
                        && rs != ConfigConstants::RS_GEOSITE_OPENAI) {
                        filtered.append(ruleVal);
                    }
                }
                rules = filtered;
            }

            route["rules"] = rules;
        }

        config["route"] = route;
    }
}

void ConfigManager::applyPortSettings(QJsonObject &config)
{
    const AppSettings &settings = AppSettings::instance();
    m_mixedPort = settings.mixedPort();
    m_apiPort = settings.apiPort();

    if (config.contains("experimental") && config["experimental"].isObject()) {
        QJsonObject experimental = config.value("experimental").toObject();
        if (experimental.contains("clash_api") && experimental["clash_api"].isObject()) {
            QJsonObject clashApi = experimental.value("clash_api").toObject();
            if (clashApi.contains("external_controller")) {
                clashApi["external_controller"] = QString("127.0.0.1:%1").arg(settings.apiPort());
            }
            experimental["clash_api"] = clashApi;
        }
        config["experimental"] = experimental;
    }

    if (config.contains("inbounds") && config["inbounds"].isArray()) {
        QJsonArray inbounds = config.value("inbounds").toArray();
        for (int i = 0; i < inbounds.size(); ++i) {
            if (!inbounds[i].isObject()) continue;
            QJsonObject inbound = inbounds[i].toObject();
            const QString type = inbound.value("type").toString();
            const QString tag = inbound.value("tag").toString();
            if ((type == "mixed" || tag == "mixed-in") && inbound.contains("listen_port")) {
                inbound["listen_port"] = settings.mixedPort();
            }
            inbounds[i] = inbound;
        }
        config["inbounds"] = inbounds;
    }
}

QJsonObject ConfigManager::buildDnsConfig()
{
    const AppSettings &settings = AppSettings::instance();
    const QString defaultOutbound = settings.normalizedDefaultOutbound();

    QJsonArray servers;

    QJsonObject dnsProxy;
    dnsProxy["tag"] = ConfigConstants::DNS_PROXY;
    dnsProxy["address"] = settings.dnsProxy();
    dnsProxy["address_resolver"] = ConfigConstants::DNS_RESOLVER;
    dnsProxy["strategy"] = settings.dnsStrategy();
    dnsProxy["detour"] = defaultOutbound;
    servers.append(dnsProxy);

    QJsonObject dnsCn;
    dnsCn["tag"] = ConfigConstants::DNS_CN;
    dnsCn["address"] = settings.dnsCn();
    dnsCn["address_resolver"] = ConfigConstants::DNS_RESOLVER;
    dnsCn["strategy"] = settings.dnsStrategy();
    dnsCn["detour"] = ConfigConstants::TAG_DIRECT;
    servers.append(dnsCn);

    QJsonObject dnsResolver;
    dnsResolver["tag"] = ConfigConstants::DNS_RESOLVER;
    dnsResolver["address"] = settings.dnsResolver();
    dnsResolver["strategy"] = settings.dnsStrategy();
    dnsResolver["detour"] = ConfigConstants::TAG_DIRECT;
    servers.append(dnsResolver);

    QJsonObject dnsBlock;
    dnsBlock["tag"] = ConfigConstants::DNS_BLOCK;
    dnsBlock["address"] = "rcode://success";
    servers.append(dnsBlock);

    QJsonArray rules;

    QJsonObject directRule;
    directRule["clash_mode"] = "direct";
    directRule["server"] = ConfigConstants::DNS_CN;
    rules.append(directRule);

    QJsonObject globalRule;
    globalRule["clash_mode"] = "global";
    globalRule["server"] = ConfigConstants::DNS_PROXY;
    rules.append(globalRule);

    if (settings.blockAds()) {
        QJsonObject adsRule;
        adsRule["rule_set"] = ConfigConstants::RS_GEOSITE_ADS;
        adsRule["server"] = ConfigConstants::DNS_BLOCK;
        rules.append(adsRule);
    }

    QJsonObject cnRule;
    QJsonArray cnSets;
    cnSets.append(ConfigConstants::RS_GEOSITE_CN);
    cnSets.append(ConfigConstants::RS_GEOIP_CN);
    cnRule["rule_set"] = cnSets;
    cnRule["server"] = ConfigConstants::DNS_CN;
    rules.append(cnRule);

    QJsonObject notCnRule;
    notCnRule["rule_set"] = ConfigConstants::RS_GEOSITE_GEOLOCATION_NOT_CN;
    notCnRule["server"] = ConfigConstants::DNS_PROXY;
    rules.append(notCnRule);

    QJsonObject dns;
    dns["servers"] = servers;
    dns["rules"] = rules;
    dns["independent_cache"] = true;
    dns["final"] = ConfigConstants::DNS_PROXY;

    return dns;
}

QJsonObject ConfigManager::buildRouteConfig()
{
    const AppSettings &settings = AppSettings::instance();
    const QString defaultOutbound = settings.normalizedDefaultOutbound();

    QJsonArray rules;

    QJsonObject sniffRule;
    sniffRule["action"] = "sniff";
    rules.append(sniffRule);

    if (settings.dnsHijack()) {
        QJsonObject hijack;
        hijack["protocol"] = "dns";
        hijack["action"] = "hijack-dns";
        rules.append(hijack);
    }

    QJsonObject globalRule;
    globalRule["clash_mode"] = "global";
    globalRule["outbound"] = defaultOutbound;
    rules.append(globalRule);

    QJsonObject directRule;
    directRule["clash_mode"] = "direct";
    directRule["outbound"] = ConfigConstants::TAG_DIRECT;
    rules.append(directRule);

    if (settings.blockAds()) {
        QJsonObject adsRule;
        adsRule["rule_set"] = ConfigConstants::RS_GEOSITE_ADS;
        adsRule["action"] = "reject";
        rules.append(adsRule);
    }

    if (settings.enableAppGroups()) {
        QJsonObject tgRule;
        tgRule["rule_set"] = ConfigConstants::RS_GEOSITE_TELEGRAM;
        tgRule["outbound"] = ConfigConstants::TAG_TELEGRAM;
        rules.append(tgRule);

        QJsonObject ytRule;
        ytRule["rule_set"] = ConfigConstants::RS_GEOSITE_YOUTUBE;
        ytRule["outbound"] = ConfigConstants::TAG_YOUTUBE;
        rules.append(ytRule);

        QJsonObject nfRule;
        nfRule["rule_set"] = ConfigConstants::RS_GEOSITE_NETFLIX;
        nfRule["outbound"] = ConfigConstants::TAG_NETFLIX;
        rules.append(nfRule);

        QJsonObject aiRule;
        aiRule["rule_set"] = ConfigConstants::RS_GEOSITE_OPENAI;
        aiRule["outbound"] = ConfigConstants::TAG_OPENAI;
        rules.append(aiRule);
    }

    QJsonObject privateRuleSet;
    privateRuleSet["rule_set"] = ConfigConstants::RS_GEOSITE_PRIVATE;
    privateRuleSet["outbound"] = ConfigConstants::TAG_DIRECT;
    rules.append(privateRuleSet);

    QJsonObject privateIpRule;
    privateIpRule["ip_cidr"] = QJsonArray::fromStringList(ConfigConstants::privateIpCidrs());
    privateIpRule["outbound"] = ConfigConstants::TAG_DIRECT;
    rules.append(privateIpRule);

    QJsonObject cnRule;
    QJsonArray cnSets;
    cnSets.append(ConfigConstants::RS_GEOSITE_CN);
    cnSets.append(ConfigConstants::RS_GEOIP_CN);
    cnRule["rule_set"] = cnSets;
    cnRule["outbound"] = ConfigConstants::TAG_DIRECT;
    rules.append(cnRule);

    QJsonObject notCnRule;
    notCnRule["rule_set"] = ConfigConstants::RS_GEOSITE_GEOLOCATION_NOT_CN;
    notCnRule["outbound"] = defaultOutbound;
    rules.append(notCnRule);

    QJsonObject route;
    route["rules"] = rules;
    route["rule_set"] = buildRuleSets();
    route["final"] = defaultOutbound;
    route["auto_detect_interface"] = true;
    route["default_domain_resolver"] = ConfigConstants::DNS_RESOLVER;

    return route;
}

QJsonArray ConfigManager::buildInbounds()
{
    const AppSettings &settings = AppSettings::instance();

    QJsonArray inbounds;

    QJsonObject mixed;
    mixed["type"] = "mixed";
    mixed["tag"] = "mixed-in";
    mixed["listen"] = "127.0.0.1";
    mixed["listen_port"] = settings.mixedPort();
    mixed["sniff"] = true;
    mixed["set_system_proxy"] = settings.systemProxyEnabled();
    inbounds.append(mixed);

    if (settings.tunEnabled()) {
        QJsonArray addresses;
        if (!settings.tunIpv4().isEmpty()) {
            addresses.append(settings.tunIpv4());
        }
        if (settings.tunEnableIpv6() && !settings.tunIpv6().isEmpty()) {
            addresses.append(settings.tunIpv6());
        }

        QJsonObject tun;
        tun["type"] = "tun";
        tun["tag"] = "tun-in";
        tun["address"] = addresses;
        tun["auto_route"] = settings.tunAutoRoute();
        tun["strict_route"] = settings.tunStrictRoute();
        tun["stack"] = settings.tunStack();
        tun["mtu"] = settings.tunMtu();
        tun["sniff"] = true;
        tun["sniff_override_destination"] = true;
        tun["route_exclude_address"] = QJsonArray::fromStringList(ConfigConstants::tunRouteExcludes());
        inbounds.append(tun);
    }

    return inbounds;
}

QJsonArray ConfigManager::buildOutboundGroups()
{
    const AppSettings &settings = AppSettings::instance();

    QJsonArray outbounds;

    QJsonObject autoOutbound;
    autoOutbound["type"] = "urltest";
    autoOutbound["tag"] = ConfigConstants::TAG_AUTO;
    autoOutbound["outbounds"] = QJsonArray::fromStringList(QStringList() << ConfigConstants::TAG_DIRECT);
    autoOutbound["url"] = settings.urltestUrl();
    autoOutbound["interrupt_exist_connections"] = true;
    autoOutbound["idle_timeout"] = "10m";
    autoOutbound["interval"] = "10m";
    autoOutbound["tolerance"] = 50;
    outbounds.append(autoOutbound);

    QJsonObject manualOutbound;
    manualOutbound["type"] = "selector";
    manualOutbound["tag"] = ConfigConstants::TAG_MANUAL;
    manualOutbound["outbounds"] = QJsonArray::fromStringList(QStringList() << ConfigConstants::TAG_AUTO);
    outbounds.append(manualOutbound);

    if (settings.enableAppGroups()) {
        QJsonArray base = QJsonArray::fromStringList(QStringList() << ConfigConstants::TAG_MANUAL
                                                                  << ConfigConstants::TAG_AUTO);
        QJsonObject tg;
        tg["type"] = "selector";
        tg["tag"] = ConfigConstants::TAG_TELEGRAM;
        tg["outbounds"] = base;
        outbounds.append(tg);

        QJsonObject yt;
        yt["type"] = "selector";
        yt["tag"] = ConfigConstants::TAG_YOUTUBE;
        yt["outbounds"] = base;
        outbounds.append(yt);

        QJsonObject nf;
        nf["type"] = "selector";
        nf["tag"] = ConfigConstants::TAG_NETFLIX;
        nf["outbounds"] = base;
        outbounds.append(nf);

        QJsonObject ai;
        ai["type"] = "selector";
        ai["tag"] = ConfigConstants::TAG_OPENAI;
        ai["outbounds"] = base;
        outbounds.append(ai);
    }

    QJsonObject direct;
    direct["type"] = "direct";
    direct["tag"] = ConfigConstants::TAG_DIRECT;
    outbounds.append(direct);

    QJsonObject block;
    block["type"] = "block";
    block["tag"] = ConfigConstants::TAG_BLOCK;
    outbounds.append(block);

    return outbounds;
}

QJsonArray ConfigManager::buildRuleSets()
{
    const AppSettings &settings = AppSettings::instance();
    const QString downloadDetour = settings.normalizedDownloadDetour();

    QJsonArray ruleSets;

    if (settings.blockAds()) {
        ruleSets.append(makeRemoteRuleSet(
            ConfigConstants::RS_GEOSITE_ADS,
            ConfigConstants::ruleSetUrl(ConfigConstants::RS_GEOSITE_ADS),
            downloadDetour,
            "1d"));
    }

    ruleSets.append(makeRemoteRuleSet(
        ConfigConstants::RS_GEOSITE_CN,
        ConfigConstants::ruleSetUrl(ConfigConstants::RS_GEOSITE_CN),
        downloadDetour,
        "1d"));

    ruleSets.append(makeRemoteRuleSet(
        ConfigConstants::RS_GEOSITE_GEOLOCATION_NOT_CN,
        ConfigConstants::ruleSetUrl(ConfigConstants::RS_GEOSITE_GEOLOCATION_NOT_CN),
        downloadDetour,
        "1d"));

    if (settings.enableAppGroups()) {
        ruleSets.append(makeRemoteRuleSet(
            ConfigConstants::RS_GEOSITE_TELEGRAM,
            ConfigConstants::ruleSetUrl(ConfigConstants::RS_GEOSITE_TELEGRAM),
            downloadDetour,
            "7d"));
        ruleSets.append(makeRemoteRuleSet(
            ConfigConstants::RS_GEOSITE_YOUTUBE,
            ConfigConstants::ruleSetUrl(ConfigConstants::RS_GEOSITE_YOUTUBE),
            downloadDetour,
            "7d"));
        ruleSets.append(makeRemoteRuleSet(
            ConfigConstants::RS_GEOSITE_NETFLIX,
            ConfigConstants::ruleSetUrl(ConfigConstants::RS_GEOSITE_NETFLIX),
            downloadDetour,
            "7d"));
        ruleSets.append(makeRemoteRuleSet(
            ConfigConstants::RS_GEOSITE_OPENAI,
            ConfigConstants::ruleSetUrl(ConfigConstants::RS_GEOSITE_OPENAI),
            downloadDetour,
            "7d"));
    }

    ruleSets.append(makeRemoteRuleSet(
        ConfigConstants::RS_GEOSITE_PRIVATE,
        ConfigConstants::ruleSetUrl(ConfigConstants::RS_GEOSITE_PRIVATE),
        ConfigConstants::TAG_DIRECT,
        "7d"));

    ruleSets.append(makeRemoteRuleSet(
        ConfigConstants::RS_GEOIP_CN,
        ConfigConstants::ruleSetUrl(ConfigConstants::RS_GEOIP_CN),
        downloadDetour,
        "1d"));

    return ruleSets;
}

QJsonObject ConfigManager::buildExperimental()
{
    const AppSettings &settings = AppSettings::instance();

    QJsonObject clashApi;
    clashApi["external_controller"] = QString("127.0.0.1:%1").arg(settings.apiPort());
    clashApi["external_ui"] = "metacubexd";
    clashApi["external_ui_download_url"] =
        "https://github.com/MetaCubeX/metacubexd/archive/refs/heads/gh-pages.zip";
    clashApi["external_ui_download_detour"] = settings.normalizedDownloadDetour();
    clashApi["default_mode"] = "rule";

    QJsonObject cacheFile;
    cacheFile["enabled"] = true;

    QJsonObject experimental;
    experimental["clash_api"] = clashApi;
    experimental["cache_file"] = cacheFile;
    return experimental;
}

bool ConfigManager::shouldIncludeNodeInGroups(const QJsonObject &node)
{
    const QString server = node.value("server").toString().trimmed();
    if (server.isEmpty()) {
        return false;
    }
    if (server == "0.0.0.0") {
        return false;
    }
    return true;
}

int ConfigManager::ensureOutboundIndex(QJsonArray &outbounds, const QString &tag)
{
    for (int i = 0; i < outbounds.size(); ++i) {
        if (!outbounds[i].isObject()) continue;
        if (outbounds[i].toObject().value("tag").toString() == tag) {
            return i;
        }
    }

    QJsonObject created;
    created["tag"] = tag;
    outbounds.append(created);
    return outbounds.size() - 1;
}

void ConfigManager::updateUrltestAndSelector(QJsonArray &outbounds, const QStringList &nodeTags)
{
    const AppSettings &settings = AppSettings::instance();

    const int autoIdx = ensureOutboundIndex(outbounds, ConfigConstants::TAG_AUTO);
    const int manualIdx = ensureOutboundIndex(outbounds, ConfigConstants::TAG_MANUAL);

    QJsonObject autoOutbound = outbounds[autoIdx].toObject();
    autoOutbound["type"] = "urltest";
    autoOutbound["tag"] = ConfigConstants::TAG_AUTO;
    if (nodeTags.isEmpty()) {
        autoOutbound["outbounds"] = QJsonArray::fromStringList(QStringList() << ConfigConstants::TAG_DIRECT);
    } else {
        autoOutbound["outbounds"] = QJsonArray::fromStringList(nodeTags);
    }
    autoOutbound["interrupt_exist_connections"] = true;
    autoOutbound["idle_timeout"] = "10m";
    autoOutbound["url"] = settings.urltestUrl();
    autoOutbound["interval"] = "10m";
    autoOutbound["tolerance"] = 50;
    outbounds[autoIdx] = autoOutbound;

    QJsonObject manualOutbound = outbounds[manualIdx].toObject();
    manualOutbound["type"] = "selector";
    manualOutbound["tag"] = ConfigConstants::TAG_MANUAL;
    QStringList manualList;
    manualList << ConfigConstants::TAG_AUTO;
    for (const auto &tag : nodeTags) {
        manualList << tag;
    }
    manualOutbound["outbounds"] = QJsonArray::fromStringList(manualList);
    outbounds[manualIdx] = manualOutbound;
}

void ConfigManager::updateAppGroupSelectors(QJsonArray &outbounds, const QStringList &nodeTags)
{
    const QStringList groups = {
        ConfigConstants::TAG_TELEGRAM,
        ConfigConstants::TAG_YOUTUBE,
        ConfigConstants::TAG_NETFLIX,
        ConfigConstants::TAG_OPENAI
    };

    for (const auto &groupTag : groups) {
        int idx = -1;
        for (int i = 0; i < outbounds.size(); ++i) {
            if (!outbounds[i].isObject()) continue;
            if (outbounds[i].toObject().value("tag").toString() == groupTag) {
                idx = i;
                break;
            }
        }
        if (idx < 0) {
            continue;
        }

        QStringList groupList;
        groupList << ConfigConstants::TAG_MANUAL << ConfigConstants::TAG_AUTO;
        for (const auto &tag : nodeTags) {
            groupList << tag;
        }

        QJsonObject group = outbounds[idx].toObject();
        group["outbounds"] = QJsonArray::fromStringList(groupList);
        outbounds[idx] = group;
    }
}
