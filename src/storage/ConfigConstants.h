#ifndef CONFIGCONSTANTS_H
#define CONFIGCONSTANTS_H

#include <QString>
#include <QStringList>

namespace ConfigConstants {

// ==================== 出站标签 ====================
// 代理组/出站标签（暴露在 Clash API，保持稳定）
const QString TAG_AUTO = "自动选择";
const QString TAG_MANUAL = "手动切换";
const QString TAG_DIRECT = "direct";
const QString TAG_BLOCK = "block";

// ==================== 业务分流组 ====================
const QString TAG_TELEGRAM = "Telegram";
const QString TAG_YOUTUBE = "YouTube";
const QString TAG_NETFLIX = "Netflix";
const QString TAG_OPENAI = "OpenAI";

// ==================== DNS 服务器标签 ====================
const QString DNS_PROXY = "dns_proxy";
const QString DNS_CN = "dns_cn";
const QString DNS_RESOLVER = "dns_resolver";
const QString DNS_BLOCK = "dns_block";

// ==================== 规则集标签 ====================
const QString RS_GEOSITE_CN = "geosite-cn";
const QString RS_GEOSITE_GEOLOCATION_NOT_CN = "geosite-geolocation-!cn";
const QString RS_GEOSITE_PRIVATE = "geosite-private";
const QString RS_GEOSITE_ADS = "geosite-category-ads-all";
const QString RS_GEOSITE_TELEGRAM = "geosite-telegram";
const QString RS_GEOSITE_YOUTUBE = "geosite-youtube";
const QString RS_GEOSITE_NETFLIX = "geosite-netflix";
const QString RS_GEOSITE_OPENAI = "geosite-openai";
const QString RS_GEOIP_CN = "geoip-cn";

// ==================== 规则集 URL ====================
// 使用 gh-proxy 加速 GitHub 访问
const QString RULE_SET_BASE = "https://raw.githubusercontent.com";

inline QString ruleSetUrl(const QString &tag) {
    if (tag == RS_GEOSITE_CN) {
        return RULE_SET_BASE + "/SagerNet/sing-geosite/rule-set/geosite-cn.srs";
    } else if (tag == RS_GEOSITE_GEOLOCATION_NOT_CN) {
        return RULE_SET_BASE + "/SagerNet/sing-geosite/rule-set/geosite-geolocation-!cn.srs";
    } else if (tag == RS_GEOSITE_PRIVATE) {
        return RULE_SET_BASE + "/SagerNet/sing-geosite/rule-set/geosite-private.srs";
    } else if (tag == RS_GEOSITE_ADS) {
        return RULE_SET_BASE + "/SagerNet/sing-geosite/rule-set/geosite-category-ads-all.srs";
    } else if (tag == RS_GEOSITE_TELEGRAM) {
        return RULE_SET_BASE + "/SagerNet/sing-geosite/rule-set/geosite-telegram.srs";
    } else if (tag == RS_GEOSITE_YOUTUBE) {
        return RULE_SET_BASE + "/SagerNet/sing-geosite/rule-set/geosite-youtube.srs";
    } else if (tag == RS_GEOSITE_NETFLIX) {
        return RULE_SET_BASE + "/SagerNet/sing-geosite/rule-set/geosite-netflix.srs";
    } else if (tag == RS_GEOSITE_OPENAI) {
        return RULE_SET_BASE + "/SagerNet/sing-geosite/rule-set/geosite-openai.srs";
    } else if (tag == RS_GEOIP_CN) {
        return RULE_SET_BASE + "/SagerNet/sing-geoip/rule-set/geoip-cn.srs";
    }
    return QString();
}

// ==================== 私网 IP 段 ====================
inline QStringList privateIpCidrs() {
    return {
        "10.0.0.0/8",
        "100.64.0.0/10",
        "127.0.0.0/8",
        "169.254.0.0/16",
        "172.16.0.0/12",
        "192.168.0.0/16",
        "::1/128",
        "fc00::/7",
        "fe80::/10"
    };
}

// ==================== TUN 排除路由 ====================
inline QStringList tunRouteExcludes() {
    return {
        "127.0.0.0/8",
        "10.0.0.0/8",
        "172.16.0.0/12",
        "192.168.0.0/16",
        "::1/128",
        "fc00::/7"
    };
}

// ==================== 默认配置 ====================
const int DEFAULT_MIXED_PORT = 7890;
const int DEFAULT_API_PORT = 9090;
const QString DEFAULT_DNS_PROXY = "1.1.1.1";
const QString DEFAULT_DNS_CN = "223.5.5.5";
const QString DEFAULT_DNS_RESOLVER = "223.5.5.5";
const QString DEFAULT_URLTEST_URL = "http://cp.cloudflare.com/generate_204";
const QString DEFAULT_TUN_IPV4 = "172.19.0.1/30";
const QString DEFAULT_TUN_IPV6 = "fdfe:dcba:9876::1/126";
const QString DEFAULT_TUN_STACK = "mixed";
const int DEFAULT_TUN_MTU = 1500;

} // namespace ConfigConstants

#endif // CONFIGCONSTANTS_H
