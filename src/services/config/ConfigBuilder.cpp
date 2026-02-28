#include "services/config/ConfigBuilder.h"
#include <QUrl>
#include <QStringList>
#include "storage/AppSettings.h"
#include "storage/ConfigConstants.h"
#include "utils/AppPaths.h"

namespace {
QJsonObject makeRemoteRuleSet(const QString& tag,
                              const QString& url,
                              const QString& downloadDetour,
                              const QString& updateInterval) {
  QJsonObject rs;
  rs["tag"]             = tag;
  rs["type"]            = "remote";
  rs["format"]          = "binary";
  rs["url"]             = url;
  rs["download_detour"] = downloadDetour;
  rs["update_interval"] = updateInterval;
  return rs;
}

QJsonObject parseDnsServerAddress(const QString& rawAddress) {
  QJsonObject dnsServer;
  const QString address = rawAddress.trimmed();
  if (address.isEmpty()) {
    return dnsServer;
  }

  if (address.compare("local", Qt::CaseInsensitive) == 0) {
    dnsServer["type"] = "local";
    return dnsServer;
  }
  if (address.compare("fakeip", Qt::CaseInsensitive) == 0) {
    dnsServer["type"] = "fakeip";
    return dnsServer;
  }

  auto fillRemoteServer = [&dnsServer](const QString& type, const QUrl& url) {
    dnsServer["type"] = type;
    QString host = url.host().trimmed();
    if (host.isEmpty()) {
      host = url.path().trimmed();
      if (host.startsWith('/')) {
        host.remove(0, 1);
      }
    }
    if (!host.isEmpty()) {
      dnsServer["server"] = host;
    }
    const int port = url.port(-1);
    if (port > 0) {
      dnsServer["server_port"] = port;
    }
    if (type == "https" || type == "h3") {
      const QString path = url.path().trimmed();
      if (!path.isEmpty() && path != "/") {
        dnsServer["path"] = path;
      }
    }
  };

  if (address.contains("://")) {
    const QUrl    url(address);
    const QString scheme = url.scheme().trimmed().toLower();
    if (scheme == "rcode") {
      return QJsonObject();
    }
    if (scheme == "udp" || scheme == "tcp" || scheme == "tls" ||
        scheme == "https" || scheme == "quic" || scheme == "h3") {
      fillRemoteServer(scheme, url);
      return dnsServer;
    }
    if (scheme == "dhcp") {
      dnsServer["type"] = "dhcp";
      QString interfaceName = url.host().trimmed();
      if (interfaceName.isEmpty()) {
        interfaceName = url.path().trimmed();
        if (interfaceName.startsWith('/')) {
          interfaceName.remove(0, 1);
        }
      }
      if (!interfaceName.isEmpty() &&
          interfaceName.compare("auto", Qt::CaseInsensitive) != 0) {
        dnsServer["interface"] = interfaceName;
      }
      return dnsServer;
    }
    return QJsonObject();
  }

  dnsServer["type"] = "udp";
  const QUrl udpUrl(QString("udp://%1").arg(address));
  const QString host = udpUrl.host().trimmed();
  if (!host.isEmpty()) {
    dnsServer["server"] = host;
    const int port = udpUrl.port(-1);
    if (port > 0) {
      dnsServer["server_port"] = port;
    }
  } else {
    dnsServer["server"] = address;
  }
  return dnsServer;
}

QJsonObject makeManagedDnsServer(const QString& tag,
                                 const QString& address,
                                 const QString& detour,
                                 bool           attachDomainResolver) {
  QJsonObject server = parseDnsServerAddress(address);
  if (server.isEmpty()) {
    server["type"]   = "udp";
    server["server"] = address.trimmed();
  }
  server["tag"] = tag;
  if (!detour.isEmpty() && detour != ConfigConstants::TAG_DIRECT) {
    server["detour"] = detour;
  }
  if (attachDomainResolver && server.contains("server")) {
    server["domain_resolver"] = ConfigConstants::DNS_RESOLVER;
  }
  return server;
}

QJsonObject makeAdsBlockDnsRule() {
  QJsonObject adsRule;
  adsRule["rule_set"] = ConfigConstants::RS_GEOSITE_ADS;
  adsRule["action"]   = "predefined";
  adsRule["rcode"]    = "NOERROR";
  return adsRule;
}
}  // namespace

QJsonObject ConfigBuilder::buildBaseConfig() {
  QJsonObject config;
  QJsonObject log;
  log["disabled"]        = false;
  log["level"]           = "info";
  log["timestamp"]       = true;
  config["log"]          = log;
  config["dns"]          = buildDnsConfig();
  config["inbounds"]     = buildInbounds();
  config["outbounds"]    = buildOutbounds();
  config["route"]        = buildRouteConfig();
  config["experimental"] = buildExperimental();
  return config;
}

QJsonObject ConfigBuilder::buildDnsConfig() {
  const AppSettings& settings        = AppSettings::instance();
  const QString      defaultOutbound = settings.normalizedDefaultOutbound();
  QJsonArray         servers;
  servers.append(makeManagedDnsServer(
      ConfigConstants::DNS_PROXY, settings.dnsProxy(), defaultOutbound, true));
  servers.append(makeManagedDnsServer(
      ConfigConstants::DNS_CN, settings.dnsCn(), ConfigConstants::TAG_DIRECT, true));
  servers.append(makeManagedDnsServer(ConfigConstants::DNS_RESOLVER,
                                      settings.dnsResolver(),
                                      ConfigConstants::TAG_DIRECT,
                                      false));
  QJsonArray  rules;
  QJsonObject directRule;
  directRule["clash_mode"] = "direct";
  directRule["server"]     = ConfigConstants::DNS_CN;
  rules.append(directRule);
  QJsonObject globalRule;
  globalRule["clash_mode"] = "global";
  globalRule["server"]     = ConfigConstants::DNS_PROXY;
  rules.append(globalRule);
  if (settings.blockAds()) {
    rules.append(makeAdsBlockDnsRule());
  }
  QJsonObject cnRule;
  QJsonArray  cnSets;
  cnSets.append(ConfigConstants::RS_GEOSITE_CN);
  cnSets.append(ConfigConstants::RS_GEOIP_CN);
  cnRule["rule_set"] = cnSets;
  cnRule["server"]   = ConfigConstants::DNS_CN;
  rules.append(cnRule);
  QJsonObject notCnRule;
  notCnRule["rule_set"] = ConfigConstants::RS_GEOSITE_GEOLOCATION_NOT_CN;
  notCnRule["server"]   = ConfigConstants::DNS_PROXY;
  rules.append(notCnRule);
  QJsonObject dns;
  dns["servers"]           = servers;
  dns["rules"]             = rules;
  dns["independent_cache"] = true;
  dns["final"]             = ConfigConstants::DNS_PROXY;
  dns["strategy"]          = settings.dnsStrategy();
  return dns;
}

QJsonObject ConfigBuilder::buildRouteConfig() {
  const AppSettings& settings        = AppSettings::instance();
  const QString      defaultOutbound = settings.normalizedDefaultOutbound();
  QJsonArray         rules;
  QJsonObject        sniffRule;
  sniffRule["action"] = "sniff";
  rules.append(sniffRule);
  if (settings.dnsHijack()) {
    QJsonObject hijack;
    hijack["protocol"] = "dns";
    hijack["action"]   = "hijack-dns";
    rules.append(hijack);
  }
  QJsonObject globalRule;
  globalRule["clash_mode"] = "global";
  globalRule["outbound"]   = defaultOutbound;
  rules.append(globalRule);
  QJsonObject directRule;
  directRule["clash_mode"] = "direct";
  directRule["outbound"]   = ConfigConstants::TAG_DIRECT;
  rules.append(directRule);
  if (settings.blockAds()) {
    QJsonObject adsRule;
    adsRule["rule_set"] = ConfigConstants::RS_GEOSITE_ADS;
    adsRule["action"]   = "reject";
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
  privateIpRule["ip_cidr"] =
      QJsonArray::fromStringList(ConfigConstants::privateIpCidrs());
  privateIpRule["outbound"] = ConfigConstants::TAG_DIRECT;
  rules.append(privateIpRule);
  QJsonObject cnRule;
  QJsonArray  cnSets;
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
  route["rules"]                   = rules;
  route["rule_set"]                = buildRuleSets();
  route["final"]                   = defaultOutbound;
  route["auto_detect_interface"]   = true;
  route["default_domain_resolver"] = ConfigConstants::DNS_RESOLVER;
  return route;
}

QJsonArray ConfigBuilder::buildInbounds() {
  const AppSettings& settings = AppSettings::instance();
  QJsonArray         inbounds;
  QJsonObject        mixed;
  mixed["type"]        = "mixed";
  mixed["tag"]         = "mixed-in";
  mixed["listen"]      = "127.0.0.1";
  mixed["listen_port"] = settings.mixedPort();
  mixed["sniff"]       = true;
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
    tun["type"]                       = "tun";
    tun["tag"]                        = "tun-in";
    tun["address"]                    = addresses;
    tun["auto_route"]                 = settings.tunAutoRoute();
    tun["strict_route"]               = settings.tunStrictRoute();
    tun["stack"]                      = settings.tunStack();
    tun["mtu"]                        = settings.tunMtu();
    tun["sniff"]                      = true;
    tun["sniff_override_destination"] =
        settings.tunSniffOverrideDestination();
    tun["route_exclude_address"] =
        QJsonArray::fromStringList(ConfigConstants::tunRouteExcludes());
    inbounds.append(tun);
  }
  return inbounds;
}

QJsonArray ConfigBuilder::buildOutbounds() {
  const AppSettings& settings = AppSettings::instance();
  QJsonArray         outbounds;
  QJsonObject        autoOutbound;
  autoOutbound["type"] = "urltest";
  autoOutbound["tag"]  = ConfigConstants::TAG_AUTO;
  autoOutbound["outbounds"] =
      QJsonArray::fromStringList(QStringList() << ConfigConstants::TAG_DIRECT);
  autoOutbound["url"]                         = settings.urltestUrl();
  autoOutbound["interrupt_exist_connections"] = true;
  autoOutbound["idle_timeout"]                = "10m";
  autoOutbound["interval"]                    = "10m";
  autoOutbound["tolerance"]                   = 50;
  outbounds.append(autoOutbound);
  QJsonObject manualOutbound;
  manualOutbound["type"] = "selector";
  manualOutbound["tag"]  = ConfigConstants::TAG_MANUAL;
  manualOutbound["outbounds"] =
      QJsonArray::fromStringList(QStringList() << ConfigConstants::TAG_AUTO);
  outbounds.append(manualOutbound);
  if (settings.enableAppGroups()) {
    const QJsonArray base =
        QJsonArray::fromStringList(QStringList() << ConfigConstants::TAG_MANUAL
                                                 << ConfigConstants::TAG_AUTO);
    // Helper lambda to create selector outbounds.
    auto createSelector = [&base](const QString& tag) {
      QJsonObject obj;
      obj["type"]      = "selector";
      obj["tag"]       = tag;
      obj["outbounds"] = base;
      return obj;
    };
    outbounds.append(createSelector(ConfigConstants::TAG_TELEGRAM));
    outbounds.append(createSelector(ConfigConstants::TAG_YOUTUBE));
    outbounds.append(createSelector(ConfigConstants::TAG_NETFLIX));
    outbounds.append(createSelector(ConfigConstants::TAG_OPENAI));
  }
  QJsonObject direct;
  direct["type"] = "direct";
  direct["tag"]  = ConfigConstants::TAG_DIRECT;
  outbounds.append(direct);
  QJsonObject block;
  block["type"] = "block";
  block["tag"]  = ConfigConstants::TAG_BLOCK;
  outbounds.append(block);
  return outbounds;
}

QJsonArray ConfigBuilder::buildRuleSets() {
  const AppSettings& settings       = AppSettings::instance();
  const QString      downloadDetour = settings.normalizedDownloadDetour();
  QJsonArray         ruleSets;
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
  ruleSets.append(
      makeRemoteRuleSet(ConfigConstants::RS_GEOSITE_GEOLOCATION_NOT_CN,
                        ConfigConstants::ruleSetUrl(
                            ConfigConstants::RS_GEOSITE_GEOLOCATION_NOT_CN),
                        downloadDetour,
                        "1d"));
  if (settings.enableAppGroups()) {
    // Helper lambda to add app-specific rule sets.
    auto addAppRuleSet = [&ruleSets, &downloadDetour](const QString& tag) {
      ruleSets.append(makeRemoteRuleSet(
          tag, ConfigConstants::ruleSetUrl(tag), downloadDetour, "7d"));
    };
    addAppRuleSet(ConfigConstants::RS_GEOSITE_TELEGRAM);
    addAppRuleSet(ConfigConstants::RS_GEOSITE_YOUTUBE);
    addAppRuleSet(ConfigConstants::RS_GEOSITE_NETFLIX);
    addAppRuleSet(ConfigConstants::RS_GEOSITE_OPENAI);
  }
  ruleSets.append(makeRemoteRuleSet(
      ConfigConstants::RS_GEOSITE_PRIVATE,
      ConfigConstants::ruleSetUrl(ConfigConstants::RS_GEOSITE_PRIVATE),
      downloadDetour,
      "7d"));
  ruleSets.append(makeRemoteRuleSet(
      ConfigConstants::RS_GEOIP_CN,
      ConfigConstants::ruleSetUrl(ConfigConstants::RS_GEOIP_CN),
      downloadDetour,
      "1d"));
  return ruleSets;
}

QJsonObject ConfigBuilder::buildExperimental() {
  const AppSettings& settings = AppSettings::instance();
  QJsonObject        clashApi;
  clashApi["external_controller"] =
      QString("127.0.0.1:%1").arg(settings.apiPort());
  clashApi["external_ui"] = "metacubexd";
  clashApi["external_ui_download_url"] =
      "https://github.com/MetaCubeX/metacubexd/archive/refs/heads/gh-pages.zip";
  clashApi["external_ui_download_detour"] = settings.normalizedDownloadDetour();
  clashApi["default_mode"]                = "rule";
  QJsonObject cacheFile;
  cacheFile["enabled"] = true;
  cacheFile["path"]    = QDir(appDataDir()).filePath("cache.db");
  QJsonObject experimental;
  experimental["clash_api"]  = clashApi;
  experimental["cache_file"] = cacheFile;
  return experimental;
}
