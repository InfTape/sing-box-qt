#include "AppSettings.h"
#include <QJsonDocument>
#include <QJsonObject>
#include "ConfigConstants.h"
#include "DatabaseService.h"
#include "utils/Logger.h"

AppSettings& AppSettings::instance() {
  static AppSettings instance;
  return instance;
}

AppSettings::AppSettings(QObject* parent)
    : QObject(parent)
      // Port defaults.
      ,
      m_mixedPort(ConfigConstants::DEFAULT_MIXED_PORT),
      m_apiPort(ConfigConstants::DEFAULT_API_PORT)
      // TUN defaults.
      ,
      m_tunEnabled(false),
      m_tunAutoRoute(true),
      m_tunStrictRoute(true),
      m_tunStack(ConfigConstants::DEFAULT_TUN_STACK),
      m_tunMtu(ConfigConstants::DEFAULT_TUN_MTU),
      m_tunIpv4(ConfigConstants::DEFAULT_TUN_IPV4),
      m_tunIpv6(ConfigConstants::DEFAULT_TUN_IPV6),
      m_tunEnableIpv6(false)
      // DNS defaults.
      ,
      m_dnsProxy(ConfigConstants::DEFAULT_DNS_PROXY),
      m_dnsCn(ConfigConstants::DEFAULT_DNS_CN),
      m_dnsResolver(ConfigConstants::DEFAULT_DNS_RESOLVER)
      // Feature flags defaults.
      ,
      m_blockAds(false),
      m_enableAppGroups(true),
      m_preferIpv6(false),
      m_dnsHijack(true),
      m_systemProxyEnabled(true),
      m_systemProxyBypass(ConfigConstants::DEFAULT_SYSTEM_PROXY_BYPASS)
      // URL test.
      ,
      m_urltestUrl(ConfigConstants::DEFAULT_URLTEST_URL),
      m_urltestTimeoutMs(ConfigConstants::DEFAULT_URLTEST_TIMEOUT_MS),
      m_urltestConcurrency(ConfigConstants::DEFAULT_URLTEST_CONCURRENCY),
      m_urltestSamples(ConfigConstants::DEFAULT_URLTEST_SAMPLES)
      // Outbound selection.
      ,
      m_defaultOutbound("manual"),
      m_downloadDetour("direct") {
  load();
}

void AppSettings::load() {
  QJsonObject config = DatabaseService::instance().getAppConfig();
  // Ports.
  m_mixedPort =
      config.value("mixedPort").toInt(ConfigConstants::DEFAULT_MIXED_PORT);
  m_apiPort = config.value("apiPort").toInt(ConfigConstants::DEFAULT_API_PORT);
  // TUN.
  m_tunEnabled     = config.value("tunEnabled").toBool(false);
  m_tunAutoRoute   = config.value("tunAutoRoute").toBool(true);
  m_tunStrictRoute = config.value("tunStrictRoute").toBool(true);
  m_tunStack =
      config.value("tunStack").toString(ConfigConstants::DEFAULT_TUN_STACK);
  m_tunMtu = config.value("tunMtu").toInt(ConfigConstants::DEFAULT_TUN_MTU);
  m_tunIpv4 =
      config.value("tunIpv4").toString(ConfigConstants::DEFAULT_TUN_IPV4);
  m_tunIpv6 =
      config.value("tunIpv6").toString(ConfigConstants::DEFAULT_TUN_IPV6);
  m_tunEnableIpv6 = config.value("tunEnableIpv6").toBool(false);
  // DNS.
  m_dnsProxy =
      config.value("dnsProxy").toString(ConfigConstants::DEFAULT_DNS_PROXY);
  m_dnsCn = config.value("dnsCn").toString(ConfigConstants::DEFAULT_DNS_CN);
  m_dnsResolver = config.value("dnsResolver")
                      .toString(ConfigConstants::DEFAULT_DNS_RESOLVER);
  // Feature flags.
  m_blockAds        = config.value("blockAds").toBool(false);
  m_enableAppGroups = config.value("enableAppGroups").toBool(true);
  m_preferIpv6      = config.value("preferIpv6").toBool(false);
  m_dnsHijack       = config.value("dnsHijack").toBool(true);
  if (config.contains("systemProxyEnabled")) {
    m_systemProxyEnabled = config.value("systemProxyEnabled").toBool(true);
  } else {
    m_systemProxyEnabled = config.value("systemProxy").toBool(true);
  }
  m_systemProxyBypass =
      config.value("systemProxyBypass")
          .toString(ConfigConstants::DEFAULT_SYSTEM_PROXY_BYPASS);
  // URL test.
  m_urltestUrl =
      config.value("urltestUrl").toString(ConfigConstants::DEFAULT_URLTEST_URL);
  m_urltestTimeoutMs = config.value("urltestTimeoutMs")
                           .toInt(ConfigConstants::DEFAULT_URLTEST_TIMEOUT_MS);
  m_urltestConcurrency =
      config.value("urltestConcurrency")
          .toInt(ConfigConstants::DEFAULT_URLTEST_CONCURRENCY);
  m_urltestSamples = config.value("urltestSamples")
                         .toInt(ConfigConstants::DEFAULT_URLTEST_SAMPLES);
  // Outbound selection.
  m_defaultOutbound = config.value("defaultOutbound").toString("manual");
  m_downloadDetour  = config.value("downloadDetour").toString("direct");
  Logger::info("App settings loaded");
}

void AppSettings::save() {
  QJsonObject config = DatabaseService::instance().getAppConfig();
  // Ports.
  config["mixedPort"] = m_mixedPort;
  config["apiPort"]   = m_apiPort;
  // TUN.
  config["tunEnabled"]     = m_tunEnabled;
  config["tunAutoRoute"]   = m_tunAutoRoute;
  config["tunStrictRoute"] = m_tunStrictRoute;
  config["tunStack"]       = m_tunStack;
  config["tunMtu"]         = m_tunMtu;
  config["tunIpv4"]        = m_tunIpv4;
  config["tunIpv6"]        = m_tunIpv6;
  config["tunEnableIpv6"]  = m_tunEnableIpv6;
  // DNS.
  config["dnsProxy"]    = m_dnsProxy;
  config["dnsCn"]       = m_dnsCn;
  config["dnsResolver"] = m_dnsResolver;
  // Feature flags.
  config["blockAds"]           = m_blockAds;
  config["enableAppGroups"]    = m_enableAppGroups;
  config["preferIpv6"]         = m_preferIpv6;
  config["dnsHijack"]          = m_dnsHijack;
  config["systemProxyEnabled"] = m_systemProxyEnabled;
  config["systemProxy"]        = m_systemProxyEnabled;
  config["systemProxyBypass"]  = m_systemProxyBypass;
  // URL test.
  config["urltestUrl"]         = m_urltestUrl;
  config["urltestTimeoutMs"]   = m_urltestTimeoutMs;
  config["urltestConcurrency"] = m_urltestConcurrency;
  config["urltestSamples"]     = m_urltestSamples;
  // Outbound selection.
  config["defaultOutbound"] = m_defaultOutbound;
  config["downloadDetour"]  = m_downloadDetour;
  DatabaseService::instance().saveAppConfig(config);
  Logger::info("App settings saved");
}

// ==================== Setter implementations ====================
void AppSettings::setMixedPort(int port) {
  if (m_mixedPort != port) {
    m_mixedPort = port;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setApiPort(int port) {
  if (m_apiPort != port) {
    m_apiPort = port;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setTunEnabled(bool enabled) {
  if (m_tunEnabled != enabled) {
    m_tunEnabled = enabled;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setTunAutoRoute(bool enabled) {
  if (m_tunAutoRoute != enabled) {
    m_tunAutoRoute = enabled;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setTunStrictRoute(bool enabled) {
  if (m_tunStrictRoute != enabled) {
    m_tunStrictRoute = enabled;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setTunStack(const QString& stack) {
  if (m_tunStack != stack) {
    m_tunStack = stack;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setTunMtu(int mtu) {
  if (m_tunMtu != mtu) {
    m_tunMtu = mtu;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setTunIpv4(const QString& addr) {
  if (m_tunIpv4 != addr) {
    m_tunIpv4 = addr;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setTunIpv6(const QString& addr) {
  if (m_tunIpv6 != addr) {
    m_tunIpv6 = addr;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setTunEnableIpv6(bool enabled) {
  if (m_tunEnableIpv6 != enabled) {
    m_tunEnableIpv6 = enabled;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setDnsProxy(const QString& dns) {
  if (m_dnsProxy != dns) {
    m_dnsProxy = dns;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setDnsCn(const QString& dns) {
  if (m_dnsCn != dns) {
    m_dnsCn = dns;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setDnsResolver(const QString& dns) {
  if (m_dnsResolver != dns) {
    m_dnsResolver = dns;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setBlockAds(bool enabled) {
  if (m_blockAds != enabled) {
    m_blockAds = enabled;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setEnableAppGroups(bool enabled) {
  if (m_enableAppGroups != enabled) {
    m_enableAppGroups = enabled;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setPreferIpv6(bool enabled) {
  if (m_preferIpv6 != enabled) {
    m_preferIpv6 = enabled;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setDnsHijack(bool enabled) {
  if (m_dnsHijack != enabled) {
    m_dnsHijack = enabled;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setSystemProxyEnabled(bool enabled) {
  if (m_systemProxyEnabled != enabled) {
    m_systemProxyEnabled = enabled;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setSystemProxyBypass(const QString& bypass) {
  if (m_systemProxyBypass != bypass) {
    m_systemProxyBypass = bypass;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setUrltestUrl(const QString& url) {
  if (m_urltestUrl != url) {
    m_urltestUrl = url;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setUrltestTimeoutMs(int ms) {
  if (ms <= 0) {
    ms = ConfigConstants::DEFAULT_URLTEST_TIMEOUT_MS;
  }
  if (m_urltestTimeoutMs != ms) {
    m_urltestTimeoutMs = ms;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setUrltestConcurrency(int c) {
  if (c <= 0) {
    c = 1;
  }
  if (m_urltestConcurrency != c) {
    m_urltestConcurrency = c;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setUrltestSamples(int s) {
  if (s <= 0) {
    s = 1;
  }
  if (m_urltestSamples != s) {
    m_urltestSamples = s;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setDefaultOutbound(const QString& outbound) {
  if (m_defaultOutbound != outbound) {
    m_defaultOutbound = outbound;
    save();
    emit settingsChanged();
  }
}

void AppSettings::setDownloadDetour(const QString& detour) {
  if (m_downloadDetour != detour) {
    m_downloadDetour = detour;
    save();
    emit settingsChanged();
  }
}

// ==================== Helper methods ====================
QString AppSettings::normalizedDefaultOutbound() const {
  if (m_defaultOutbound == "auto") {
    return ConfigConstants::TAG_AUTO;
  }
  return ConfigConstants::TAG_MANUAL;
}

QString AppSettings::normalizedDownloadDetour() const {
  if (m_downloadDetour == "manual") {
    return ConfigConstants::TAG_MANUAL;
  }
  return ConfigConstants::TAG_DIRECT;
}

QString AppSettings::dnsStrategy() const {
  if (m_preferIpv6) {
    return "prefer_ipv6";
  }
  return "ipv4_only";
}
