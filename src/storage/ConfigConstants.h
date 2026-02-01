#ifndef CONFIGCONSTANTS_H
#define CONFIGCONSTANTS_H

#include <QString>
#include <QStringList>
namespace ConfigConstants {

// ==================== Outbound tags ====================
// Proxy group/outbound tags exposed in Clash API (keep stable)
const QString TAG_AUTO   = "auto";
const QString TAG_MANUAL = "manual";
const QString TAG_DIRECT = "direct";
const QString TAG_BLOCK  = "block";

// ==================== App routing groups ====================
const QString TAG_TELEGRAM = "Telegram";
const QString TAG_YOUTUBE  = "YouTube";
const QString TAG_NETFLIX  = "Netflix";
const QString TAG_OPENAI   = "OpenAI";

// ==================== DNS server tags ====================
const QString DNS_PROXY    = "dns_proxy";
const QString DNS_CN       = "dns_cn";
const QString DNS_RESOLVER = "dns_resolver";
const QString DNS_BLOCK    = "dns_block";

// ==================== Rule set tags ====================
const QString RS_GEOSITE_CN                 = "geosite-cn";
const QString RS_GEOSITE_GEOLOCATION_NOT_CN = "geosite-geolocation-!cn";
const QString RS_GEOSITE_PRIVATE            = "geosite-private";
const QString RS_GEOSITE_ADS                = "geosite-category-ads-all";
const QString RS_GEOSITE_TELEGRAM           = "geosite-telegram";
const QString RS_GEOSITE_YOUTUBE            = "geosite-youtube";
const QString RS_GEOSITE_NETFLIX            = "geosite-netflix";
const QString RS_GEOSITE_OPENAI             = "geosite-openai";
const QString RS_GEOIP_CN                   = "geoip-cn";

// ==================== Rule set URLs ====================
const QString  RULE_SET_BASE = "https://raw.githubusercontent.com";
inline QString ruleSetUrl(const QString& tag) {
  if (tag == RS_GEOSITE_CN) {
    return RULE_SET_BASE + "/SagerNet/sing-geosite/rule-set/geosite-cn.srs";
  } else if (tag == RS_GEOSITE_GEOLOCATION_NOT_CN) {
    return RULE_SET_BASE +
           "/SagerNet/sing-geosite/rule-set/geosite-geolocation-!cn.srs";
  } else if (tag == RS_GEOSITE_PRIVATE) {
    return RULE_SET_BASE +
           "/SagerNet/sing-geosite/rule-set/geosite-private.srs";
  } else if (tag == RS_GEOSITE_ADS) {
    return RULE_SET_BASE +
           "/SagerNet/sing-geosite/rule-set/geosite-category-ads-all.srs";
  } else if (tag == RS_GEOSITE_TELEGRAM) {
    return RULE_SET_BASE +
           "/SagerNet/sing-geosite/rule-set/geosite-telegram.srs";
  } else if (tag == RS_GEOSITE_YOUTUBE) {
    return RULE_SET_BASE +
           "/SagerNet/sing-geosite/rule-set/geosite-youtube.srs";
  } else if (tag == RS_GEOSITE_NETFLIX) {
    return RULE_SET_BASE +
           "/SagerNet/sing-geosite/rule-set/geosite-netflix.srs";
  } else if (tag == RS_GEOSITE_OPENAI) {
    return RULE_SET_BASE + "/SagerNet/sing-geosite/rule-set/geosite-openai.srs";
  } else if (tag == RS_GEOIP_CN) {
    return RULE_SET_BASE + "/SagerNet/sing-geoip/rule-set/geoip-cn.srs";
  }
  return QString();
}
// ==================== Private IP ranges ====================
inline QStringList privateIpCidrs() {
  return {"10.0.0.0/8",     "100.64.0.0/10", "127.0.0.0/8",
          "169.254.0.0/16", "172.16.0.0/12", "192.168.0.0/16",
          "::1/128",        "fc00::/7",      "fe80::/10"};
}
// ==================== TUN route excludes ====================
inline QStringList tunRouteExcludes() {
  return {"127.0.0.0/8",    "10.0.0.0/8", "172.16.0.0/12",
          "192.168.0.0/16", "::1/128",    "fc00::/7"};
}
// ==================== Default configuration ====================
const int     DEFAULT_MIXED_PORT   = 7890;
const int     DEFAULT_API_PORT     = 9090;
const QString DEFAULT_DNS_PROXY    = "https://1.1.1.1/dns-query";
const QString DEFAULT_DNS_CN       = "h3://dns.alidns.com/dns-query";
const QString DEFAULT_DNS_RESOLVER = "114.114.114.114";
// 与 Throne-dev 保持一致的默认延迟测试地址
const QString DEFAULT_URLTEST_URL = "http://cp.cloudflare.com/";
const QString DEFAULT_SYSTEM_PROXY_BYPASS =
    "localhost;127.*;10.*;172.16.*;172.17.*;172.18.*;172.19.*;172.20.*;172.21.*"
    ";172.22.*;172.23.*;172.24.*;172.25.*;172.26.*;172.27.*;172.28.*;172.29.*;"
    "172.30.*;172.31.*;192.168.*";
const QString DEFAULT_TUN_IPV4  = "172.19.0.1/30";
const QString DEFAULT_TUN_IPV6  = "fdfe:dcba:9876::1/126";
const QString DEFAULT_TUN_STACK = "mixed";
const int     DEFAULT_TUN_MTU   = 1500;

// ==================== URL test defaults ====================
// Throne-dev 默认值：3s 超时、10 并发、双次采样
const int DEFAULT_URLTEST_TIMEOUT_MS  = 3000;
const int DEFAULT_URLTEST_CONCURRENCY = 10;
const int DEFAULT_URLTEST_SAMPLES     = 2;

}  // namespace ConfigConstants
#endif  // CONFIGCONSTANTS_H
