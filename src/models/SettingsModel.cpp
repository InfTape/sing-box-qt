#include "SettingsModel.h"
#include "storage/AppSettings.h"
#include "storage/ConfigConstants.h"
#include "storage/DatabaseService.h"

SettingsModel::Data SettingsModel::load() {
  SettingsModel::Data d;
  QJsonObject         config = DatabaseService::instance().getAppConfig();
  d.mixedPort =
      config.value("mixedPort").toInt(ConfigConstants::DEFAULT_MIXED_PORT);
  d.apiPort = config.value("apiPort").toInt(ConfigConstants::DEFAULT_API_PORT);
  d.autoStart = config.value("autoStart").toBool(false);
  if (config.contains("systemProxyEnabled")) {
    d.systemProxyEnabled = config.value("systemProxyEnabled").toBool(true);
  } else {
    d.systemProxyEnabled = config.value("systemProxy").toBool(true);
  }
  d.systemProxyBypass =
      config.value("systemProxyBypass")
          .toString(ConfigConstants::DEFAULT_SYSTEM_PROXY_BYPASS);
  d.tunMtu = config.value("tunMtu").toInt(ConfigConstants::DEFAULT_TUN_MTU);
  d.tunStack =
      config.value("tunStack").toString(ConfigConstants::DEFAULT_TUN_STACK);
  d.tunEnableIpv6   = config.value("tunEnableIpv6").toBool(false);
  d.tunAutoRoute    = config.value("tunAutoRoute").toBool(true);
  d.tunStrictRoute  = config.value("tunStrictRoute").toBool(true);
  d.defaultOutbound = config.value("defaultOutbound").toString("manual");
  d.downloadDetour  = config.value("downloadDetour").toString("direct");
  d.blockAds        = config.value("blockAds").toBool(false);
  d.dnsHijack       = config.value("dnsHijack").toBool(true);
  d.enableAppGroups = config.value("enableAppGroups").toBool(true);
  d.dnsProxy =
      config.value("dnsProxy").toString(ConfigConstants::DEFAULT_DNS_PROXY);
  d.dnsCn = config.value("dnsCn").toString(ConfigConstants::DEFAULT_DNS_CN);
  d.dnsResolver = config.value("dnsResolver")
                      .toString(ConfigConstants::DEFAULT_DNS_RESOLVER);
  d.urltestUrl =
      config.value("urltestUrl").toString(ConfigConstants::DEFAULT_URLTEST_URL);
  return d;
}

bool SettingsModel::save(const SettingsModel::Data& data,
                         QString*                   errorMessage) {
  if (data.tunMtu < 576 || data.tunMtu > 9000) {
    if (errorMessage) {
      *errorMessage = QObject::tr("MTU must be between 576 and 9000");
    }
    return false;
  }
  if (data.systemProxyBypass.trimmed().isEmpty()) {
    if (errorMessage) {
      *errorMessage = QObject::tr("Please enter system proxy bypass domains");
    }
    return false;
  }
  QJsonObject config           = DatabaseService::instance().getAppConfig();
  config["mixedPort"]          = data.mixedPort;
  config["apiPort"]            = data.apiPort;
  config["autoStart"]          = data.autoStart;
  config["systemProxyEnabled"] = data.systemProxyEnabled;
  config["systemProxy"]        = data.systemProxyEnabled;
  config["systemProxyBypass"]  = data.systemProxyBypass;
  config["tunMtu"]             = data.tunMtu;
  config["tunStack"]           = data.tunStack;
  config["tunEnableIpv6"]      = data.tunEnableIpv6;
  config["tunAutoRoute"]       = data.tunAutoRoute;
  config["tunStrictRoute"]     = data.tunStrictRoute;
  config["defaultOutbound"]    = data.defaultOutbound;
  config["downloadDetour"]     = data.downloadDetour;
  config["blockAds"]           = data.blockAds;
  config["dnsHijack"]          = data.dnsHijack;
  config["enableAppGroups"]    = data.enableAppGroups;
  config["dnsProxy"]           = data.dnsProxy;
  config["dnsCn"]              = data.dnsCn;
  config["dnsResolver"]        = data.dnsResolver;
  config["urltestUrl"]         = data.urltestUrl;
  if (!DatabaseService::instance().saveAppConfig(config)) {
    if (errorMessage) {
      *errorMessage = QObject::tr("Failed to save settings");
    }
    return false;
  }
  AppSettings::instance().load();
  return true;
}
