#include <QObject>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include "network/SubscriptionService.h"
#include "services/config/ConfigBuilder.h"
#include "services/config/ConfigMutator.h"
#include "services/subscription/SubscriptionParser.h"
#include "services/kernel/KernelPlatform.h"
#include "storage/AppSettings.h"
#include "storage/ConfigConstants.h"
#include "storage/DatabaseService.h"
#include "utils/Crypto.h"
#include "utils/LogParser.h"
#include "utils/home/HomeFormat.h"
#include "utils/proxy/ProxyNodeHelper.h"
#include "utils/rule/RuleUtils.h"
#include "utils/settings/SettingsHelpers.h"
#include "utils/subscription/SubscriptionHelpers.h"
#include "utils/subscription/SubscriptionFormat.h"
#include "utils/subscription/SubscriptionUserinfo.h"

namespace {
QJsonObject findObjectByTag(const QJsonArray& arr, const QString& tag) {
  for (const auto& v : arr) {
    if (!v.isObject()) {
      continue;
    }
    const QJsonObject obj = v.toObject();
    if (obj.value("tag").toString() == tag) {
      return obj;
    }
  }
  return QJsonObject();
}

int findRuleSetIndex(const QJsonArray& rules, const QString& ruleSet) {
  for (int i = 0; i < rules.size(); ++i) {
    if (!rules[i].isObject()) {
      continue;
    }
    if (rules[i].toObject().value("rule_set").toString() == ruleSet) {
      return i;
    }
  }
  return -1;
}

int findTagIndex(const QJsonArray& arr, const QString& tag) {
  for (int i = 0; i < arr.size(); ++i) {
    if (!arr[i].isObject()) {
      continue;
    }
    if (arr[i].toObject().value("tag").toString() == tag) {
      return i;
    }
  }
  return -1;
}

int findProtocolActionIndex(const QJsonArray& rules,
                            const QString&    protocol,
                            const QString&    action) {
  for (int i = 0; i < rules.size(); ++i) {
    if (!rules[i].isObject()) {
      continue;
    }
    const QJsonObject obj = rules[i].toObject();
    if (obj.value("protocol").toString() == protocol &&
        obj.value("action").toString() == action) {
      return i;
    }
  }
  return -1;
}

struct AppSettingsScopeGuard {
  AppSettings& s = AppSettings::instance();

  int     mixedPort;
  int     apiPort;
  bool    tunEnabled;
  bool    tunAutoRoute;
  bool    tunStrictRoute;
  QString tunStack;
  int     tunMtu;
  QString tunIpv4;
  QString tunIpv6;
  bool    tunEnableIpv6;
  QString dnsProxy;
  QString dnsCn;
  QString dnsResolver;
  bool    blockAds;
  bool    enableAppGroups;
  bool    preferIpv6;
  bool    dnsHijack;
  bool    systemProxyEnabled;
  QString systemProxyBypass;
  QString urltestUrl;
  int     urltestTimeoutMs;
  int     urltestConcurrency;
  int     urltestSamples;
  QString defaultOutbound;
  QString downloadDetour;

  AppSettingsScopeGuard()
      : mixedPort(s.mixedPort()),
        apiPort(s.apiPort()),
        tunEnabled(s.tunEnabled()),
        tunAutoRoute(s.tunAutoRoute()),
        tunStrictRoute(s.tunStrictRoute()),
        tunStack(s.tunStack()),
        tunMtu(s.tunMtu()),
        tunIpv4(s.tunIpv4()),
        tunIpv6(s.tunIpv6()),
        tunEnableIpv6(s.tunEnableIpv6()),
        dnsProxy(s.dnsProxy()),
        dnsCn(s.dnsCn()),
        dnsResolver(s.dnsResolver()),
        blockAds(s.blockAds()),
        enableAppGroups(s.enableAppGroups()),
        preferIpv6(s.preferIpv6()),
        dnsHijack(s.dnsHijack()),
        systemProxyEnabled(s.systemProxyEnabled()),
        systemProxyBypass(s.systemProxyBypass()),
        urltestUrl(s.urltestUrl()),
        urltestTimeoutMs(s.urltestTimeoutMs()),
        urltestConcurrency(s.urltestConcurrency()),
        urltestSamples(s.urltestSamples()),
        defaultOutbound(s.defaultOutbound()),
        downloadDetour(s.downloadDetour()) {}

  ~AppSettingsScopeGuard() {
    s.setMixedPort(mixedPort);
    s.setApiPort(apiPort);
    s.setTunEnabled(tunEnabled);
    s.setTunAutoRoute(tunAutoRoute);
    s.setTunStrictRoute(tunStrictRoute);
    s.setTunStack(tunStack);
    s.setTunMtu(tunMtu);
    s.setTunIpv4(tunIpv4);
    s.setTunIpv6(tunIpv6);
    s.setTunEnableIpv6(tunEnableIpv6);
    s.setDnsProxy(dnsProxy);
    s.setDnsCn(dnsCn);
    s.setDnsResolver(dnsResolver);
    s.setBlockAds(blockAds);
    s.setEnableAppGroups(enableAppGroups);
    s.setPreferIpv6(preferIpv6);
    s.setDnsHijack(dnsHijack);
    s.setSystemProxyEnabled(systemProxyEnabled);
    s.setSystemProxyBypass(systemProxyBypass);
    s.setUrltestUrl(urltestUrl);
    s.setUrltestTimeoutMs(urltestTimeoutMs);
    s.setUrltestConcurrency(urltestConcurrency);
    s.setUrltestSamples(urltestSamples);
    s.setDefaultOutbound(defaultOutbound);
    s.setDownloadDetour(downloadDetour);
  }
};
}  // namespace

class UnitUtilsTest : public QObject {
  Q_OBJECT

 private slots:
  void ruleUtils_shouldNormalizeTypeAndProxy();
  void ruleUtils_shouldDetectCustomPayload();
  void ruleUtils_shouldHandleAdditionalProxyShapes();
  void homeFormat_shouldFormatBytesAndDuration();
  void homeFormat_shouldCoverAdditionalUnits();
  void proxyNodeHelper_shouldMapDelayState();
  void proxyNodeHelper_shouldHandleAdditionalDelayForms();
  void logParser_shouldParseAndDetectTypes();
  void logParser_shouldHandleFallbackPaths();
  void logParser_shouldMapLabels();
  void subscriptionUserinfo_shouldParseHeader();
  void subscriptionUserinfo_shouldIgnoreInvalidSegments();
  void subscriptionFormat_shouldFormatFields();
  void subscriptionFormat_shouldHandleAdditionalRanges();
  void crypto_shouldEncodeDecodeAndHash();
  void settingsHelpers_shouldMapModesAndNormalizeText();
  void subscriptionHelpers_shouldDetectSingleManualNode();
  void kernelPlatform_shouldBuildUrlsAndFilename();
  void kernelPlatform_shouldHandlePathUtilities();
  void initTestCase();
  void configBuilder_shouldBuildFeatureEnabledBaseConfig();
  void configBuilder_shouldBuildMinimalBaseConfigWhenFeaturesDisabled();
  void configMutator_shouldUpdateAndReadDefaultMode();
  void configMutator_shouldApplySharedRules();
  void configMutator_shouldInjectNodesAndUpdateSelectors();
  void configMutator_shouldApplyPortSettingsAndFeatureRemovals();
  void configMutator_shouldApplySettingsFeatureInsertions();
  void subscriptionParser_shouldParseCommonUris();
  void subscriptionParser_shouldParseWireguardConfig();
  void subscriptionParser_shouldHandleBase64Fallbacks();
  void subscriptionParser_shouldParseAdvancedProtocols();
  void subscriptionParser_shouldParseClashSip008AndSingleJson();
  void subscriptionParser_shouldParseSingBoxAndMixedUriList();
};

void UnitUtilsTest::initTestCase() {
  (void)DatabaseService::instance().init();
}

void UnitUtilsTest::configBuilder_shouldBuildFeatureEnabledBaseConfig() {
  AppSettingsScopeGuard guard;
  AppSettings&          settings = AppSettings::instance();
  settings.setBlockAds(true);
  settings.setEnableAppGroups(true);
  settings.setDnsHijack(true);
  settings.setPreferIpv6(true);
  settings.setTunEnabled(true);
  settings.setTunAutoRoute(true);
  settings.setTunStrictRoute(true);
  settings.setTunStack("mixed");
  settings.setTunMtu(1380);
  settings.setTunIpv4("172.19.0.1/30");
  settings.setTunEnableIpv6(true);
  settings.setTunIpv6("fdfe::1/126");
  settings.setMixedPort(2080);
  settings.setApiPort(29090);
  settings.setDefaultOutbound("auto");
  settings.setDownloadDetour("manual");
  settings.setUrltestUrl("http://example.com/ping");

  const QJsonObject config = ConfigBuilder::buildBaseConfig();

  const QJsonObject logObj = config.value("log").toObject();
  QVERIFY(!logObj.value("disabled").toBool());
  QCOMPARE(logObj.value("level").toString(), QString("info"));

  const QJsonArray inbounds = config.value("inbounds").toArray();
  QCOMPARE(inbounds.size(), 2);
  QCOMPARE(findObjectByTag(inbounds, "mixed-in").value("listen_port").toInt(),
           2080);
  const QJsonObject tunInbound = findObjectByTag(inbounds, "tun-in");
  QCOMPARE(tunInbound.value("type").toString(), QString("tun"));
  QCOMPARE(tunInbound.value("address").toArray().size(), 2);
  QCOMPARE(tunInbound.value("stack").toString(), QString("mixed"));
  QCOMPARE(tunInbound.value("mtu").toInt(), 1380);

  const QJsonObject dnsObj = config.value("dns").toObject();
  QCOMPARE(dnsObj.value("final").toString(), ConfigConstants::DNS_PROXY);
  const QJsonArray dnsServers = dnsObj.value("servers").toArray();
  QCOMPARE(findObjectByTag(dnsServers, ConfigConstants::DNS_PROXY)
               .value("detour")
               .toString(),
           ConfigConstants::TAG_AUTO);
  QCOMPARE(findObjectByTag(dnsServers, ConfigConstants::DNS_PROXY)
               .value("strategy")
               .toString(),
           QString("prefer_ipv6"));
  const QJsonArray dnsRules = dnsObj.value("rules").toArray();
  QVERIFY(findRuleSetIndex(dnsRules, ConfigConstants::RS_GEOSITE_ADS) >= 0);

  const QJsonArray outbounds = config.value("outbounds").toArray();
  QCOMPARE(findObjectByTag(outbounds, ConfigConstants::TAG_AUTO)
               .value("url")
               .toString(),
           QString("http://example.com/ping"));
  QVERIFY(!findObjectByTag(outbounds, ConfigConstants::TAG_TELEGRAM).isEmpty());
  QVERIFY(!findObjectByTag(outbounds, ConfigConstants::TAG_OPENAI).isEmpty());

  const QJsonObject routeObj = config.value("route").toObject();
  QCOMPARE(routeObj.value("final").toString(), ConfigConstants::TAG_AUTO);
  const QJsonArray routeRules = routeObj.value("rules").toArray();
  QVERIFY(findProtocolActionIndex(routeRules, "dns", "hijack-dns") >= 0);
  QVERIFY(findRuleSetIndex(routeRules, ConfigConstants::RS_GEOSITE_ADS) >= 0);
  QVERIFY(findRuleSetIndex(routeRules, ConfigConstants::RS_GEOSITE_TELEGRAM) >=
          0);
  const QJsonArray ruleSets = routeObj.value("rule_set").toArray();
  const int        adsIdx = findTagIndex(ruleSets, ConfigConstants::RS_GEOSITE_ADS);
  const int tgIdx =
      findTagIndex(ruleSets, ConfigConstants::RS_GEOSITE_TELEGRAM);
  const int cnIdx = findTagIndex(ruleSets, ConfigConstants::RS_GEOSITE_CN);
  const int privateIdx =
      findTagIndex(ruleSets, ConfigConstants::RS_GEOSITE_PRIVATE);
  QVERIFY(adsIdx >= 0);
  QVERIFY(tgIdx >= 0);
  QVERIFY(cnIdx >= 0);
  QVERIFY(privateIdx >= 0);
  QCOMPARE(ruleSets[cnIdx].toObject().value("download_detour").toString(),
           ConfigConstants::TAG_MANUAL);
  QCOMPARE(ruleSets[privateIdx].toObject().value("download_detour").toString(),
           ConfigConstants::TAG_DIRECT);

  const QJsonObject experimental = config.value("experimental").toObject();
  const QJsonObject clashApi = experimental.value("clash_api").toObject();
  QCOMPARE(clashApi.value("external_controller").toString(),
           QString("127.0.0.1:29090"));
  QCOMPARE(clashApi.value("external_ui_download_detour").toString(),
           ConfigConstants::TAG_MANUAL);
  QVERIFY(!experimental.value("cache_file")
               .toObject()
               .value("path")
               .toString()
               .isEmpty());
}

void UnitUtilsTest::configBuilder_shouldBuildMinimalBaseConfigWhenFeaturesDisabled() {
  AppSettingsScopeGuard guard;
  AppSettings&          settings = AppSettings::instance();
  settings.setBlockAds(false);
  settings.setEnableAppGroups(false);
  settings.setDnsHijack(false);
  settings.setPreferIpv6(false);
  settings.setTunEnabled(false);
  settings.setDefaultOutbound("manual");
  settings.setDownloadDetour("direct");
  settings.setUrltestUrl("http://example.com/min");

  const QJsonObject config = ConfigBuilder::buildBaseConfig();

  const QJsonArray inbounds = config.value("inbounds").toArray();
  QCOMPARE(inbounds.size(), 1);
  QVERIFY(findObjectByTag(inbounds, "tun-in").isEmpty());

  const QJsonObject dnsObj = config.value("dns").toObject();
  const QJsonArray  dnsRules = dnsObj.value("rules").toArray();
  QCOMPARE(findRuleSetIndex(dnsRules, ConfigConstants::RS_GEOSITE_ADS), -1);
  QCOMPARE(findObjectByTag(dnsObj.value("servers").toArray(), ConfigConstants::DNS_PROXY)
               .value("detour")
               .toString(),
           ConfigConstants::TAG_MANUAL);

  const QJsonArray outbounds = config.value("outbounds").toArray();
  QVERIFY(findObjectByTag(outbounds, ConfigConstants::TAG_TELEGRAM).isEmpty());
  QVERIFY(findObjectByTag(outbounds, ConfigConstants::TAG_YOUTUBE).isEmpty());
  QVERIFY(findObjectByTag(outbounds, ConfigConstants::TAG_NETFLIX).isEmpty());
  QVERIFY(findObjectByTag(outbounds, ConfigConstants::TAG_OPENAI).isEmpty());

  const QJsonObject routeObj = config.value("route").toObject();
  QCOMPARE(routeObj.value("final").toString(), ConfigConstants::TAG_MANUAL);
  const QJsonArray routeRules = routeObj.value("rules").toArray();
  QCOMPARE(findProtocolActionIndex(routeRules, "dns", "hijack-dns"), -1);
  QCOMPARE(findRuleSetIndex(routeRules, ConfigConstants::RS_GEOSITE_ADS), -1);
  QCOMPARE(findRuleSetIndex(routeRules, ConfigConstants::RS_GEOSITE_TELEGRAM),
           -1);
  const QJsonArray ruleSets = routeObj.value("rule_set").toArray();
  QCOMPARE(findTagIndex(ruleSets, ConfigConstants::RS_GEOSITE_ADS), -1);
  QCOMPARE(findTagIndex(ruleSets, ConfigConstants::RS_GEOSITE_TELEGRAM), -1);

  const QJsonObject clashApi =
      config.value("experimental").toObject().value("clash_api").toObject();
  QCOMPARE(clashApi.value("external_ui_download_detour").toString(),
           ConfigConstants::TAG_DIRECT);
}

void UnitUtilsTest::ruleUtils_shouldNormalizeTypeAndProxy() {
  QCOMPARE(RuleUtils::normalizeRuleTypeKey("  DOMAIN  "), QString("domain"));
  QCOMPARE(RuleUtils::normalizeRuleTypeKey("  "), QString("default"));

  QVERIFY(!RuleUtils::displayRuleTypeLabel("").isEmpty());
  QCOMPARE(RuleUtils::displayRuleTypeLabel("domain"), QString("domain"));

  QCOMPARE(RuleUtils::normalizeProxyValue("DIRECT"), QString("direct"));
  QCOMPARE(RuleUtils::normalizeProxyValue("Reject"), QString("reject"));
  QCOMPARE(RuleUtils::normalizeProxyValue("[Proxy(Node-A)]"), QString("Node-A"));
  QCOMPARE(RuleUtils::normalizeProxyValue("route(node-b)"), QString("node-b"));

  QVERIFY(!RuleUtils::displayProxyLabel("direct").isEmpty());
  QVERIFY(!RuleUtils::displayProxyLabel("reject").isEmpty());
}

void UnitUtilsTest::ruleUtils_shouldDetectCustomPayload() {
  QVERIFY(RuleUtils::isCustomPayload("domain_suffix=example.com"));
  QVERIFY(RuleUtils::isCustomPayload("IP_CIDR=1.1.1.0/24"));
  QVERIFY(RuleUtils::isCustomPayload("process_name=foo.exe"));
  QVERIFY(RuleUtils::isCustomPayload("package_name=com.demo.app"));
  QVERIFY(RuleUtils::isCustomPayload("port=443"));
  QVERIFY(RuleUtils::isCustomPayload("source=192.168.0.1"));
  QVERIFY(!RuleUtils::isCustomPayload("geoip-cn"));
}

void UnitUtilsTest::ruleUtils_shouldHandleAdditionalProxyShapes() {
  QCOMPARE(RuleUtils::normalizeProxyValue("[route(node-c)]"), QString("node-c"));
  QCOMPARE(RuleUtils::normalizeProxyValue("Proxy(Node-D)"), QString("Node-D"));
  QCOMPARE(RuleUtils::normalizeProxyValue("[Node-E]"), QString("Node-E"));
  QCOMPARE(RuleUtils::displayProxyLabel("Node-F"), QString("Node-F"));
}

void UnitUtilsTest::homeFormat_shouldFormatBytesAndDuration() {
  QCOMPARE(HomeFormat::bytes(0), QString("0 B"));
  QCOMPARE(HomeFormat::bytes(1), QString("1 B"));
  QCOMPARE(HomeFormat::bytes(1024), QString("1.00 KB"));
  QCOMPARE(HomeFormat::bytes(1536), QString("1.50 KB"));

  QCOMPARE(HomeFormat::duration(59), QString("00:59"));
  QCOMPARE(HomeFormat::duration(3600), QString("1:00:00"));
  QCOMPARE(HomeFormat::duration(3661), QString("1:01:01"));
}

void UnitUtilsTest::homeFormat_shouldCoverAdditionalUnits() {
  QCOMPARE(HomeFormat::bytes(1048576), QString("1.00 MB"));
  QCOMPARE(HomeFormat::bytes(1073741824), QString("1.00 GB"));
  QCOMPARE(HomeFormat::bytes(1099511627776), QString("1.00 TB"));

  QCOMPARE(HomeFormat::duration(0), QString("00:00"));
  QCOMPARE(HomeFormat::duration(61), QString("01:01"));
}

void UnitUtilsTest::proxyNodeHelper_shouldMapDelayState() {
  QCOMPARE(ProxyNodeHelper::delayStateFromText(""), QString("loading"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("..."), QString("loading"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("90 ms"), QString("ok"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("120 ms"), QString("warn"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("300 ms"), QString("bad"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("-1"), QString("bad"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("not-a-number"), QString(""));
}

void UnitUtilsTest::proxyNodeHelper_shouldHandleAdditionalDelayForms() {
  QCOMPARE(ProxyNodeHelper::delayStateFromText("0 ms"), QString("bad"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("100 ms"), QString("warn"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("299 ms"), QString("warn"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("250"), QString("warn"));
}

void UnitUtilsTest::logParser_shouldParseAndDetectTypes() {
  QCOMPARE(LogParser::stripAnsiSequences("\x1B[31mERROR\x1B[0m test"),
           QString("ERROR test"));

  QCOMPARE(LogParser::detectLogType("panic happened"), QString("panic"));
  QCOMPARE(LogParser::detectLogType("FATAL: fail"), QString("fatal"));
  QCOMPARE(LogParser::detectLogType("error happened"), QString("error"));
  QCOMPARE(LogParser::detectLogType("warning message"), QString("warning"));
  QCOMPARE(LogParser::detectLogType("debug trace"), QString("debug"));
  QCOMPARE(LogParser::detectLogType("TRACE packet"), QString("trace"));
  QCOMPARE(LogParser::detectLogType("info message"), QString("info"));

  const LogParser::LogKind dns = LogParser::parseLogKind("dns: query example");
  QVERIFY(dns.isDns);
  QCOMPARE(dns.direction, QString("dns"));
  QVERIFY(!dns.isConnection);

  const LogParser::LogKind inbound =
      LogParser::parseLogKind("inbound connection from 127.0.0.1:12345");
  QVERIFY(inbound.isConnection);
  QCOMPARE(inbound.direction, QString("inbound"));
  QCOMPARE(inbound.host, QString("127.0.0.1:12345"));

  const LogParser::LogKind outbound = LogParser::parseLogKind(
      "outbound connection to 1.1.1.1:443 outbound/vmess[Node-A]");
  QVERIFY(outbound.isConnection);
  QCOMPARE(outbound.direction, QString("outbound"));
  QCOMPARE(outbound.host, QString("1.1.1.1:443"));
  QCOMPARE(outbound.protocol, QString("vmess"));
  QCOMPARE(outbound.nodeName, QString("Node-A"));
}

void UnitUtilsTest::logParser_shouldHandleFallbackPaths() {
  const LogParser::LogKind unknown = LogParser::parseLogKind("just a plain log");
  QVERIFY(!unknown.isConnection);
  QVERIFY(!unknown.isDns);
  QVERIFY(unknown.direction.isEmpty());
  QVERIFY(unknown.host.isEmpty());

  const LogParser::LogKind outboundNoNode =
      LogParser::parseLogKind("outbound connection to 8.8.8.8:53");
  QVERIFY(outboundNoNode.isConnection);
  QCOMPARE(outboundNoNode.direction, QString("outbound"));
  QCOMPARE(outboundNoNode.host, QString("8.8.8.8:53"));
  QVERIFY(outboundNoNode.protocol.isEmpty());
  QVERIFY(outboundNoNode.nodeName.isEmpty());

  QCOMPARE(LogParser::detectLogType("plain message"), QString("info"));
}

void UnitUtilsTest::logParser_shouldMapLabels() {
  QCOMPARE(LogParser::logTypeLabel("trace"), QString("TRACE"));
  QCOMPARE(LogParser::logTypeLabel("debug"), QString("DEBUG"));
  QCOMPARE(LogParser::logTypeLabel("info"), QString("INFO"));
  QCOMPARE(LogParser::logTypeLabel("warning"), QString("WARN"));
  QCOMPARE(LogParser::logTypeLabel("error"), QString("ERROR"));
  QCOMPARE(LogParser::logTypeLabel("fatal"), QString("FATAL"));
  QCOMPARE(LogParser::logTypeLabel("panic"), QString("PANIC"));
  QCOMPARE(LogParser::logTypeLabel("unknown"), QString("INFO"));
}

void UnitUtilsTest::subscriptionUserinfo_shouldParseHeader() {
  const QByteArray goodHeader =
      "upload=1024; download=2048; total=4096; expire=1735689600";
  const QJsonObject info = SubscriptionUserinfo::parseUserinfoHeader(goodHeader);
  QCOMPARE(info.value("upload").toInteger(), 1024);
  QCOMPARE(info.value("download").toInteger(), 2048);
  QCOMPARE(info.value("total").toInteger(), 4096);
  QCOMPARE(info.value("expire").toInteger(), 1735689600);

  const QJsonObject mixed = SubscriptionUserinfo::parseUserinfoHeader(
      "UPLOAD=1; unknown=10; download=-1; total=2");
  QCOMPARE(mixed.value("upload").toInteger(), 1);
  QVERIFY(!mixed.contains("download"));
  QCOMPARE(mixed.value("total").toInteger(), 2);
  QVERIFY(!mixed.contains("unknown"));

  QVERIFY(SubscriptionUserinfo::parseUserinfoHeader(QByteArray()).isEmpty());
}

void UnitUtilsTest::subscriptionUserinfo_shouldIgnoreInvalidSegments() {
  const QJsonObject invalid = SubscriptionUserinfo::parseUserinfoHeader(
      "upload=10=20;justtext;download=abc;expire=-1;total=5");
  QVERIFY(!invalid.contains("upload"));
  QCOMPARE(invalid.value("download").toInteger(), 0);
  QVERIFY(!invalid.contains("expire"));
  QCOMPARE(invalid.value("total").toInteger(), 5);
}

void UnitUtilsTest::subscriptionFormat_shouldFormatFields() {
  QCOMPARE(SubscriptionFormat::formatBytes(0), QString("0 B"));
  QCOMPARE(SubscriptionFormat::formatBytes(1024), QString("1.00 KB"));

  QVERIFY(!SubscriptionFormat::formatTimestamp(0).isEmpty());
  const QString ts = SubscriptionFormat::formatTimestamp(1700000000000);
  QVERIFY(QRegularExpression("^\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}$")
              .match(ts)
              .hasMatch());

  QCOMPARE(SubscriptionFormat::formatExpireTime(0), QString());
  QVERIFY(SubscriptionFormat::formatExpireTime(1700000000)
              .startsWith(QString("Expires: ")));
}

void UnitUtilsTest::subscriptionFormat_shouldHandleAdditionalRanges() {
  QCOMPARE(SubscriptionFormat::formatBytes(1024LL * 1024), QString("1.00 MB"));
  QCOMPARE(SubscriptionFormat::formatBytes(1024LL * 1024 * 1024),
           QString("1.00 GB"));

  QVERIFY(!SubscriptionFormat::formatTimestamp(1).isEmpty());
  QCOMPARE(SubscriptionFormat::formatExpireTime(-1), QString());
}

void UnitUtilsTest::crypto_shouldEncodeDecodeAndHash() {
  const QByteArray raw("hello/world+=");
  const QString    b64 = Crypto::base64Encode(raw);
  QCOMPARE(Crypto::base64Decode(b64), raw);

  const QString b64Url = Crypto::base64UrlEncode(raw);
  QVERIFY(!b64Url.contains('+'));
  QVERIFY(!b64Url.contains('/'));
  QVERIFY(!b64Url.contains('='));
  QCOMPARE(Crypto::base64UrlDecode(b64Url), raw);

  QCOMPARE(Crypto::sha256(QStringLiteral("abc")),
           QString("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));

  const QString uuid = Crypto::generateUUID();
  QVERIFY(QRegularExpression(
              "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$")
              .match(uuid)
              .hasMatch());
}

void UnitUtilsTest::settingsHelpers_shouldMapModesAndNormalizeText() {
  QCOMPARE(SettingsHelpers::themeIndexFromMode(ThemeService::ThemeMode::Dark),
           0);
  QCOMPARE(SettingsHelpers::themeIndexFromMode(ThemeService::ThemeMode::Light),
           1);
  QCOMPARE(SettingsHelpers::themeIndexFromMode(ThemeService::ThemeMode::Auto),
           2);

  QCOMPARE(SettingsHelpers::themeModeFromIndex(0), ThemeService::ThemeMode::Dark);
  QCOMPARE(SettingsHelpers::themeModeFromIndex(1),
           ThemeService::ThemeMode::Light);
  QCOMPARE(SettingsHelpers::themeModeFromIndex(2), ThemeService::ThemeMode::Auto);
  QCOMPARE(SettingsHelpers::themeModeFromIndex(99),
           ThemeService::ThemeMode::Dark);

  QCOMPARE(SettingsHelpers::normalizeBypassText("a\r\nb\nc"), QString("a;b;c"));
  QCOMPARE(SettingsHelpers::resolveTextOrDefault(nullptr, "fallback"),
           QString("fallback"));
}

void UnitUtilsTest::subscriptionHelpers_shouldDetectSingleManualNode() {
  SubscriptionInfo info;
  info.isManual          = false;
  info.useOriginalConfig = false;
  info.manualContent     = R"([{"type":"vmess","server":"a.com"}])";
  QVERIFY(!SubscriptionHelpers::isSingleManualNode(info, nullptr));

  info.isManual          = true;
  info.useOriginalConfig = true;
  QVERIFY(!SubscriptionHelpers::isSingleManualNode(info, nullptr));

  info.useOriginalConfig = false;
  QJsonObject outNode;
  QVERIFY(SubscriptionHelpers::isSingleManualNode(info, &outNode));
  QCOMPARE(outNode.value("type").toString(), QString("vmess"));
  QCOMPARE(outNode.value("server").toString(), QString("a.com"));

  info.manualContent = R"({"type":"trojan","server":"b.com"})";
  outNode            = QJsonObject();
  QVERIFY(SubscriptionHelpers::isSingleManualNode(info, &outNode));
  QCOMPARE(outNode.value("type").toString(), QString("trojan"));

  info.manualContent = R"({"type":"trojan"})";
  QVERIFY(!SubscriptionHelpers::isSingleManualNode(info, nullptr));

  info.manualContent = R"([{"type":"a","server":"s1"},{"type":"b","server":"s2"}])";
  QVERIFY(!SubscriptionHelpers::isSingleManualNode(info, nullptr));
}

void UnitUtilsTest::kernelPlatform_shouldBuildUrlsAndFilename() {
  const QString arch = KernelPlatform::getKernelArch();
  QVERIFY(arch == "amd64" || arch == "arm64" || arch == "386");

  const QString filename = KernelPlatform::buildKernelFilename("v1.2.3");
  QVERIFY(filename.contains("1.2.3"));
  QVERIFY(filename.contains("windows"));
  QVERIFY(filename.endsWith(".zip"));
  QVERIFY(!filename.contains("v1.2.3"));

  const QStringList urls =
      KernelPlatform::buildDownloadUrls("1.2.3", "sing-box-1.2.3-windows-amd64.zip");
  QCOMPARE(urls.size(), 4);
  QVERIFY(urls[0].startsWith("https://ghproxy.net/"));
  QVERIFY(urls[1].startsWith("https://mirror.ghproxy.com/"));
  QVERIFY(urls[2].startsWith("https://ghproxy.com/"));
  QVERIFY(urls[3].startsWith("https://github.com/"));
  QVERIFY(urls[3].contains("/download/v1.2.3/"));
  QVERIFY(urls[3].contains("sing-box-1.2.3-windows-amd64.zip"));
}

void UnitUtilsTest::kernelPlatform_shouldHandlePathUtilities() {
  QCOMPARE(KernelPlatform::queryKernelVersion(""), QString());
  QCOMPARE(KernelPlatform::queryKernelVersion("C:/definitely/not/exist.exe"),
           QString());

  QTemporaryDir tmpDir;
  QVERIFY(tmpDir.isValid());
  const QString nestedDir = tmpDir.filePath("a/b");
  QVERIFY(QDir().mkpath(nestedDir));
  const QString exePath = nestedDir + "/my-kernel.exe";
  QFile         f(exePath);
  QVERIFY(f.open(QIODevice::WriteOnly));
  f.write("dummy");
  f.close();

  const QString found =
      KernelPlatform::findExecutableInDir(tmpDir.path(), "my-kernel.exe");
  QCOMPARE(QDir::fromNativeSeparators(found),
           QDir::fromNativeSeparators(exePath));

  QString err;
  QVERIFY(!KernelPlatform::extractZipArchive(tmpDir.filePath("missing.zip"),
                                             tmpDir.filePath("out"),
                                             &err));
  QVERIFY(!err.isEmpty());
}

void UnitUtilsTest::configMutator_shouldUpdateAndReadDefaultMode() {
  QJsonObject config;
  QString     error;
  QVERIFY(!ConfigMutator::updateClashDefaultMode(config, "invalid-mode", &error));
  QVERIFY(error.contains("Invalid proxy mode"));

  QVERIFY(ConfigMutator::updateClashDefaultMode(config, " Global ", &error));
  QCOMPARE(ConfigMutator::readClashDefaultMode(config), QString("global"));

  const QJsonObject experimental = config.value("experimental").toObject();
  const QJsonObject clashApi     = experimental.value("clash_api").toObject();
  QCOMPARE(clashApi.value("default_mode").toString(), QString("global"));
  QVERIFY(!clashApi.value("external_ui").toString().isEmpty());
  QVERIFY(experimental.value("cache_file")
              .toObject()
              .value("path")
              .toString()
              .endsWith("cache.db"));

  QVERIFY(ConfigMutator::updateClashDefaultMode(config, "rule", nullptr));
  QCOMPARE(ConfigMutator::readClashDefaultMode(config), QString("rule"));
  QCOMPARE(ConfigMutator::readClashDefaultMode(QJsonObject()), QString("rule"));
}

void UnitUtilsTest::configMutator_shouldApplySharedRules() {
  QJsonObject config;
  QJsonObject route;
  QJsonArray  rules;
  rules.append(QJsonObject{{"clash_mode", "global"}});
  rules.append(QJsonObject{{"clash_mode", "direct"}});
  rules.append(
      QJsonObject{{"rule_set", "foo"}, {"outbound", "manual"}, {"shared", true}});
  rules.append(QJsonObject{{"rule_set", "x"}, {"outbound", "direct"}});
  route["rules"]  = rules;
  config["route"] = route;

  QJsonArray sharedRules;
  sharedRules.append(QJsonObject{{"rule_set", "foo"},
                                 {"outbound", "manual"},
                                 {"shared", true},
                                 {"source", "shared-store"}});
  sharedRules.append(QJsonObject{{"rule_set", "z"}, {"outbound", "manual"}});
  sharedRules.append(QJsonObject{{"rule_set", "z"}, {"outbound", "manual"}});

  ConfigMutator::applySharedRules(config, sharedRules, true);
  QJsonArray enabledRules = config.value("route").toObject().value("rules").toArray();
  QCOMPARE(enabledRules.size(), 5);
  QCOMPARE(enabledRules[0].toObject().value("clash_mode").toString(),
           QString("global"));
  QCOMPARE(enabledRules[1].toObject().value("clash_mode").toString(),
           QString("direct"));
  QCOMPARE(enabledRules[2].toObject().value("rule_set").toString(), QString("foo"));
  QCOMPARE(enabledRules[3].toObject().value("rule_set").toString(), QString("z"));
  QCOMPARE(enabledRules[4].toObject().value("rule_set").toString(), QString("x"));

  ConfigMutator::applySharedRules(config, sharedRules, false);
  QJsonArray disabledRules =
      config.value("route").toObject().value("rules").toArray();
  QCOMPARE(disabledRules.size(), 3);
  QCOMPARE(disabledRules[0].toObject().value("clash_mode").toString(),
           QString("global"));
  QCOMPARE(disabledRules[1].toObject().value("clash_mode").toString(),
           QString("direct"));
  QCOMPARE(disabledRules[2].toObject().value("rule_set").toString(), QString("x"));
}

void UnitUtilsTest::configMutator_shouldInjectNodesAndUpdateSelectors() {
  QJsonObject config;
  QJsonArray  outbounds;
  outbounds.append(QJsonObject{{"tag", ConfigConstants::TAG_AUTO}});
  outbounds.append(QJsonObject{{"tag", ConfigConstants::TAG_MANUAL}});
  outbounds.append(QJsonObject{{"tag", ConfigConstants::TAG_TELEGRAM}});
  outbounds.append(QJsonObject{{"tag", "existing"}});
  config["outbounds"] = outbounds;

  QJsonArray nodes;
  nodes.append(123);  // invalid: non-object
  nodes.append(QJsonObject{{"tag", ""}, {"type", "vmess"}, {"server", "a.com"}});
  nodes.append(
      QJsonObject{{"tag", "existing"}, {"type", "vmess"}, {"server", "example.com"}});
  nodes.append(
      QJsonObject{{"tag", "lan"}, {"type", "vmess"}, {"server", "0.0.0.0"}});
  nodes.append(QJsonObject{{"tag", "ip"}, {"type", "vmess"}, {"server", "8.8.8.8"}});
  nodes.append(QJsonObject{{"tag", "notype"}, {"server", "b.com"}});  // invalid

  QVERIFY(ConfigMutator::injectNodes(config, nodes));
  const QJsonArray result = config.value("outbounds").toArray();

  QJsonObject renamedNode;
  QJsonObject lanNode;
  QJsonObject ipNode;
  for (const auto& v : result) {
    if (!v.isObject()) {
      continue;
    }
    const QJsonObject obj = v.toObject();
    const QString     tag = obj.value("tag").toString();
    if (obj.value("server").toString() == "example.com") {
      renamedNode = obj;
    } else if (tag == "lan") {
      lanNode = obj;
    } else if (tag == "ip") {
      ipNode = obj;
    }
  }

  QVERIFY(!renamedNode.isEmpty());
  QVERIFY(renamedNode.value("tag").toString().startsWith("node-existing-"));
  QVERIFY(renamedNode.contains("domain_resolver"));
  QCOMPARE(renamedNode.value("domain_resolver")
               .toObject()
               .value("server")
               .toString(),
           ConfigConstants::DNS_RESOLVER);

  QVERIFY(!lanNode.isEmpty());
  QVERIFY(!lanNode.contains("domain_resolver"));

  QVERIFY(!ipNode.isEmpty());
  QVERIFY(!ipNode.contains("domain_resolver"));

  const QString renamedTag = renamedNode.value("tag").toString();
  const QJsonObject autoObj =
      findObjectByTag(result, ConfigConstants::TAG_AUTO);
  const QJsonObject manualObj =
      findObjectByTag(result, ConfigConstants::TAG_MANUAL);
  const QJsonObject tgObj = findObjectByTag(result, ConfigConstants::TAG_TELEGRAM);

  const QJsonArray autoList = autoObj.value("outbounds").toArray();
  const QJsonArray manualList = manualObj.value("outbounds").toArray();
  const QJsonArray tgList = tgObj.value("outbounds").toArray();

  QCOMPARE(autoList.size(), 2);
  QCOMPARE(autoList[0].toString(), renamedTag);
  QCOMPARE(autoList[1].toString(), QString("ip"));

  QCOMPARE(manualList.size(), 3);
  QCOMPARE(manualList[0].toString(), ConfigConstants::TAG_AUTO);
  QCOMPARE(manualList[1].toString(), renamedTag);
  QCOMPARE(manualList[2].toString(), QString("ip"));

  QCOMPARE(tgList.size(), 4);
  QCOMPARE(tgList[0].toString(), ConfigConstants::TAG_MANUAL);
  QCOMPARE(tgList[1].toString(), ConfigConstants::TAG_AUTO);
  QCOMPARE(tgList[2].toString(), renamedTag);
  QCOMPARE(tgList[3].toString(), QString("ip"));
}

void UnitUtilsTest::subscriptionParser_shouldParseCommonUris() {
  const QString vlessUri =
      "vless://123e4567-e89b-12d3-a456-426614174000@example.com:8443"
      "?type=ws&path=%2Fws&host=cdn.example.com&sni=sni.example.com"
      "&fp=chrome&alpn=h2,h3#Node-VLESS";
  const QJsonObject vless = SubscriptionParser::parseVlessURI(vlessUri);
  QCOMPARE(vless.value("type").toString(), QString("vless"));
  QCOMPARE(vless.value("server").toString(), QString("example.com"));
  QCOMPARE(vless.value("server_port").toInt(), 8443);
  QCOMPARE(vless.value("uuid").toString(),
           QString("123e4567-e89b-12d3-a456-426614174000"));
  QCOMPARE(vless.value("tag").toString(), QString("Node-VLESS"));
  QCOMPARE(vless.value("transport").toObject().value("type").toString(),
           QString("ws"));
  QCOMPARE(vless.value("transport").toObject().value("path").toString(),
           QString("/ws"));
  QCOMPARE(vless.value("transport")
               .toObject()
               .value("headers")
               .toObject()
               .value("Host")
               .toString(),
           QString("cdn.example.com"));
  QVERIFY(vless.value("tls").toObject().value("enabled").toBool());

  const QJsonObject http = SubscriptionParser::parseHttpURI(
      "https://user:pass@proxy.example.com#Proxy-HTTP");
  QCOMPARE(http.value("type").toString(), QString("http"));
  QCOMPARE(http.value("server").toString(), QString("proxy.example.com"));
  QCOMPARE(http.value("server_port").toInt(), 443);
  QCOMPARE(http.value("username").toString(), QString("user"));
  QCOMPARE(http.value("password").toString(), QString("pass"));
  QVERIFY(http.value("tls").toObject().value("enabled").toBool());

  const QJsonObject socks =
      SubscriptionParser::parseSocksURI("socks://u:p@1.2.3.4:1080#Proxy-SOCKS");
  QCOMPARE(socks.value("type").toString(), QString("socks"));
  QCOMPARE(socks.value("server").toString(), QString("1.2.3.4"));
  QCOMPARE(socks.value("server_port").toInt(), 1080);
  QCOMPARE(socks.value("username").toString(), QString("u"));
  QCOMPARE(socks.value("password").toString(), QString("p"));
}

void UnitUtilsTest::subscriptionParser_shouldParseWireguardConfig() {
  const QString wgConfig =
      "[Interface]\n"
      "PrivateKey = private-key\n"
      "Address = 10.0.0.2/32, fd00::2/128\n"
      "Description = WG-Node\n"
      "\n"
      "[Peer]\n"
      "PublicKey = public-key\n"
      "PresharedKey = pre-shared\n"
      "Endpoint = wg.example.com:51820\n";
  const QJsonObject node = SubscriptionParser::parseWireguardConfig(wgConfig);
  QCOMPARE(node.value("type").toString(), QString("wireguard"));
  QCOMPARE(node.value("server").toString(), QString("wg.example.com"));
  QCOMPARE(node.value("server_port").toInt(), 51820);
  QCOMPARE(node.value("private_key").toString(), QString("private-key"));
  QCOMPARE(node.value("peer_public_key").toString(), QString("public-key"));
  QCOMPARE(node.value("pre_shared_key").toString(), QString("pre-shared"));
  QCOMPARE(node.value("tag").toString(), QString("WG-Node"));
  const QJsonArray localAddr = node.value("local_address").toArray();
  QCOMPARE(localAddr.size(), 2);
  QCOMPARE(localAddr[0].toString(), QString("10.0.0.2/32"));
  QCOMPARE(localAddr[1].toString(), QString("fd00::2/128"));
}

void UnitUtilsTest::subscriptionParser_shouldHandleBase64Fallbacks() {
  const QString plain = "http://example.com:8080#H1";
  const QString encoded = QString::fromLatin1(plain.toUtf8().toBase64());
  QCOMPARE(SubscriptionParser::tryDecodeBase64ToText(encoded), plain);
  QVERIFY(SubscriptionParser::tryDecodeBase64ToText("%%%invalid%%%").isEmpty());

  const QJsonArray decodedNodes =
      SubscriptionParser::extractNodesWithFallback(encoded);
  QCOMPARE(decodedNodes.size(), 1);
  QCOMPARE(decodedNodes[0].toObject().value("type").toString(), QString("http"));
  QCOMPARE(decodedNodes[0].toObject().value("server").toString(),
           QString("example.com"));

  const QString strippedEncoded =
      "vmess://" + QString::fromLatin1(plain.toUtf8().toBase64());
  const QJsonArray strippedNodes =
      SubscriptionParser::extractNodesWithFallback(strippedEncoded);
  QCOMPARE(strippedNodes.size(), 1);
  QCOMPARE(strippedNodes[0].toObject().value("type").toString(),
           QString("http"));
}

void UnitUtilsTest::configMutator_shouldApplyPortSettingsAndFeatureRemovals() {
  AppSettingsScopeGuard guard;
  AppSettings&          settings = AppSettings::instance();
  settings.setApiPort(9999);
  settings.setMixedPort(8899);
  settings.setBlockAds(false);
  settings.setEnableAppGroups(false);
  settings.setDnsHijack(false);
  settings.setDefaultOutbound("manual");
  settings.setDownloadDetour("direct");
  settings.setPreferIpv6(false);
  settings.setDnsProxy("https://1.0.0.1/dns-query");
  settings.setDnsCn("h3://dns.alidns.com/dns-query");
  settings.setDnsResolver("223.5.5.5");
  settings.setUrltestUrl("http://cp.cloudflare.com/");

  QJsonObject config;
  config["experimental"] =
      QJsonObject{{"clash_api", QJsonObject{{"external_controller", "127.0.0.1:1111"}}}};
  config["inbounds"] =
      QJsonArray{QJsonObject{{"type", "mixed"}, {"listen_port", 1234}},
                 QJsonObject{{"tag", "mixed-in"}, {"listen_port", 5678}},
                 QJsonObject{{"type", "mixed"}},
                 QJsonObject{{"type", "tun"}, {"listen_port", 9990}}};

  config["dns"] = QJsonObject{
      {"servers",
       QJsonArray{
           QJsonObject{{"tag", ConfigConstants::DNS_PROXY},
                       {"address", "old-proxy"},
                       {"detour", "old"}},
           QJsonObject{{"tag", ConfigConstants::DNS_CN}, {"address", "old-cn"}},
           QJsonObject{{"tag", ConfigConstants::DNS_RESOLVER},
                       {"address", "old-resolver"}},
       }},
      {"rules",
       QJsonArray{
           QJsonObject{{"rule_set", ConfigConstants::RS_GEOSITE_ADS},
                       {"server", ConfigConstants::DNS_BLOCK}},
           QJsonObject{{"clash_mode", "global"},
                       {"server", ConfigConstants::DNS_PROXY}},
       }}};

  config["outbounds"] =
      QJsonArray{QJsonObject{{"tag", ConfigConstants::TAG_AUTO}},
                 QJsonObject{{"tag", ConfigConstants::TAG_MANUAL}},
                 QJsonObject{{"tag", ConfigConstants::TAG_TELEGRAM}},
                 QJsonObject{{"tag", ConfigConstants::TAG_YOUTUBE}},
                 QJsonObject{{"tag", ConfigConstants::TAG_NETFLIX}},
                 QJsonObject{{"tag", ConfigConstants::TAG_OPENAI}}};

  config["route"] = QJsonObject{
      {"rule_set",
       QJsonArray{
           QJsonObject{{"type", "remote"},
                       {"tag", ConfigConstants::RS_GEOSITE_ADS},
                       {"download_detour", "old"}},
           QJsonObject{{"type", "remote"},
                       {"tag", ConfigConstants::RS_GEOSITE_TELEGRAM},
                       {"download_detour", "old"}},
           QJsonObject{{"type", "remote"},
                       {"tag", ConfigConstants::RS_GEOSITE_GEOLOCATION_NOT_CN},
                       {"download_detour", "old"}},
       }},
      {"rules",
       QJsonArray{
           QJsonObject{{"clash_mode", "global"}, {"outbound", "auto"}},
           QJsonObject{{"protocol", "dns"}, {"action", "hijack-dns"}},
           QJsonObject{{"rule_set", ConfigConstants::RS_GEOSITE_ADS},
                       {"action", "reject"}},
           QJsonObject{{"rule_set", ConfigConstants::RS_GEOSITE_TELEGRAM},
                       {"outbound", ConfigConstants::TAG_TELEGRAM}},
           QJsonObject{
               {"rule_set", ConfigConstants::RS_GEOSITE_GEOLOCATION_NOT_CN},
               {"outbound", "old"}},
       }},
      {"final", "auto"}};

  ConfigMutator::applyPortSettings(config);
  QCOMPARE(config.value("experimental")
               .toObject()
               .value("clash_api")
               .toObject()
               .value("external_controller")
               .toString(),
           QString("127.0.0.1:9999"));
  const QJsonArray portInbounds = config.value("inbounds").toArray();
  QCOMPARE(portInbounds[0].toObject().value("listen_port").toInt(), 8899);
  QCOMPARE(portInbounds[1].toObject().value("listen_port").toInt(), 8899);
  QVERIFY(!portInbounds[2].toObject().contains("listen_port"));
  QCOMPARE(portInbounds[3].toObject().value("listen_port").toInt(), 9990);

  ConfigMutator::applySettings(config);

  const QJsonObject dnsObj = config.value("dns").toObject();
  QCOMPARE(dnsObj.value("strategy").toString(), QString("ipv4_only"));
  const QJsonArray dnsServers = dnsObj.value("servers").toArray();
  QCOMPARE(findObjectByTag(dnsServers, ConfigConstants::DNS_PROXY)
               .value("address")
               .toString(),
           QString("https://1.0.0.1/dns-query"));
  QCOMPARE(findObjectByTag(dnsServers, ConfigConstants::DNS_PROXY)
               .value("detour")
               .toString(),
           ConfigConstants::TAG_MANUAL);
  QCOMPARE(findObjectByTag(dnsServers, ConfigConstants::DNS_CN)
               .value("address")
               .toString(),
           QString("h3://dns.alidns.com/dns-query"));
  QCOMPARE(findObjectByTag(dnsServers, ConfigConstants::DNS_RESOLVER)
               .value("address")
               .toString(),
           QString("223.5.5.5"));

  const QJsonArray dnsRules = dnsObj.value("rules").toArray();
  QCOMPARE(findRuleSetIndex(dnsRules, ConfigConstants::RS_GEOSITE_ADS), -1);

  const QJsonArray outbounds = config.value("outbounds").toArray();
  QVERIFY(findObjectByTag(outbounds, ConfigConstants::TAG_AUTO)
              .value("url")
              .toString()
              .contains("cloudflare"));
  QVERIFY(findObjectByTag(outbounds, ConfigConstants::TAG_TELEGRAM).isEmpty());
  QVERIFY(findObjectByTag(outbounds, ConfigConstants::TAG_OPENAI).isEmpty());

  const QJsonObject routeObj = config.value("route").toObject();
  QCOMPARE(routeObj.value("final").toString(), ConfigConstants::TAG_MANUAL);
  const QJsonArray ruleSets = routeObj.value("rule_set").toArray();
  QCOMPARE(findTagIndex(ruleSets, ConfigConstants::RS_GEOSITE_ADS), -1);
  QCOMPARE(findTagIndex(ruleSets, ConfigConstants::RS_GEOSITE_TELEGRAM), -1);

  const QJsonArray routeRules = routeObj.value("rules").toArray();
  QCOMPARE(findProtocolActionIndex(routeRules, "dns", "hijack-dns"), -1);
  QCOMPARE(findRuleSetIndex(routeRules, ConfigConstants::RS_GEOSITE_ADS), -1);
  QCOMPARE(findRuleSetIndex(routeRules, ConfigConstants::RS_GEOSITE_TELEGRAM),
           -1);
}

void UnitUtilsTest::configMutator_shouldApplySettingsFeatureInsertions() {
  AppSettingsScopeGuard guard;
  AppSettings&          settings = AppSettings::instance();
  settings.setBlockAds(true);
  settings.setEnableAppGroups(true);
  settings.setDnsHijack(true);
  settings.setPreferIpv6(true);
  settings.setDefaultOutbound("auto");
  settings.setDownloadDetour("manual");
  settings.setUrltestUrl("http://example.com/test");

  QJsonObject config;
  config["dns"] = QJsonObject{
      {"rules",
       QJsonArray{QJsonObject{{"clash_mode", "global"},
                              {"server", ConfigConstants::DNS_PROXY}}}}};
  config["outbounds"] =
      QJsonArray{QJsonObject{{"tag", ConfigConstants::TAG_AUTO}},
                 QJsonObject{{"tag", ConfigConstants::TAG_MANUAL}},
                 QJsonObject{{"tag", ConfigConstants::TAG_TELEGRAM}},
                 QJsonObject{{"tag", ConfigConstants::TAG_YOUTUBE}},
                 QJsonObject{{"tag", ConfigConstants::TAG_NETFLIX}},
                 QJsonObject{{"tag", ConfigConstants::TAG_OPENAI}}};
  config["route"] = QJsonObject{
      {"rule_set",
       QJsonArray{QJsonObject{{"type", "remote"},
                              {"tag", ConfigConstants::RS_GEOSITE_CN},
                              {"download_detour", "old"}}}},
      {"rules",
       QJsonArray{
           QJsonObject{{"clash_mode", "global"}, {"outbound", "manual"}},
       }}};

  ConfigMutator::applySettings(config);

  const QJsonObject dnsObj = config.value("dns").toObject();
  QCOMPARE(dnsObj.value("strategy").toString(), QString("prefer_ipv6"));
  const QJsonArray dnsRules = dnsObj.value("rules").toArray();
  QVERIFY(findRuleSetIndex(dnsRules, ConfigConstants::RS_GEOSITE_ADS) >= 0);

  const QJsonObject routeObj = config.value("route").toObject();
  QCOMPARE(routeObj.value("final").toString(), ConfigConstants::TAG_AUTO);

  const QJsonArray routeRules = routeObj.value("rules").toArray();
  QVERIFY(findProtocolActionIndex(routeRules, "dns", "hijack-dns") >= 0);
  QVERIFY(findRuleSetIndex(routeRules, ConfigConstants::RS_GEOSITE_ADS) >= 0);

  const QJsonArray ruleSets = routeObj.value("rule_set").toArray();
  const int cnIdx = findTagIndex(ruleSets, ConfigConstants::RS_GEOSITE_CN);
  QVERIFY(cnIdx >= 0);
  QCOMPARE(ruleSets[cnIdx]
               .toObject()
               .value("download_detour")
               .toString(),
           ConfigConstants::TAG_MANUAL);
}

void UnitUtilsTest::subscriptionParser_shouldParseAdvancedProtocols() {
  const QJsonObject vmessWs = SubscriptionParser::parseVmessURI(
      "vmess://"
      "eyJ2IjoiMiIsInBzIjoiVk0tV1MiLCJhZGQiOiJ2bS5leGFtcGxlLmNvbSIsInBvcnQiOiI4"
      "NDQzIiwiaWQiOiIxMjNlNDU2Ny1lODliLTEyZDMtYTQ1Ni00MjY2MTQxNzQwMDAiLCJhaWQiOi"
      "IwIiwibmV0Ijoid3MiLCJob3N0IjoiY2RuLmV4YW1wbGUuY29tIiwicGF0aCI6Ii93cyIsInRs"
      "cyI6InRscyIsInNuaSI6InNuaS5leGFtcGxlLmNvbSIsImFscG4iOiJoMixoMyIsImZwIjoiY2"
      "hyb21lIiwiYWxsb3dJbnNlY3VyZSI6IjEifQ==");
  QCOMPARE(vmessWs.value("type").toString(), QString("vmess"));
  QCOMPARE(vmessWs.value("tag").toString(), QString("VM-WS"));
  QCOMPARE(vmessWs.value("transport").toObject().value("type").toString(),
           QString("ws"));
  QVERIFY(vmessWs.value("tls").toObject().value("insecure").toBool());

  const QJsonObject vmessGrpc = SubscriptionParser::parseVmessURI(
      "vmess://"
      "eyJhZGQiOiJ2bTIuZXhhbXBsZS5jb20iLCJwb3J0Ijo0NDMsImlkIjoiMTIzZTQ1NjctZTg5"
      "Yi0xMmQzLWE0NTYtNDI2NjE0MTc0MDAwIiwibmV0IjoiZ3JwYyIsInBhdGgiOiJzdmMtZnJvbS"
      "1wYXRoIiwidGxzIjoidGxzIn0=");
  QCOMPARE(vmessGrpc.value("transport").toObject().value("type").toString(),
           QString("grpc"));
  QCOMPARE(
      vmessGrpc.value("transport").toObject().value("service_name").toString(),
      QString("svc-from-path"));

  const QString ssUserInfo =
      QString::fromLatin1(QString("aes-128-gcm:pwd123").toUtf8().toBase64());
  const QJsonObject ss1 = SubscriptionParser::parseShadowsocksURI(
      QString("ss://%1@ss.example.com:8388#SS1").arg(ssUserInfo));
  QCOMPARE(ss1.value("type").toString(), QString("shadowsocks"));
  QCOMPARE(ss1.value("method").toString(), QString("aes-128-gcm"));
  QCOMPARE(ss1.value("password").toString(), QString("pwd123"));
  QCOMPARE(ss1.value("server").toString(), QString("ss.example.com"));

  const QString ssFull = QString::fromLatin1(
      QString("chacha20-ietf-poly1305:pass@ss2.example.com:443")
          .toUtf8()
          .toBase64());
  const QJsonObject ss2 =
      SubscriptionParser::parseShadowsocksURI(QString("ss://%1").arg(ssFull));
  QCOMPARE(ss2.value("type").toString(), QString("shadowsocks"));
  QCOMPARE(ss2.value("server").toString(), QString("ss2.example.com"));
  QCOMPARE(ss2.value("server_port").toInt(), 443);

  const QJsonObject trojan = SubscriptionParser::parseTrojanURI(
      "trojan://pass@trojan.example.com:443?sni=cdn.example.com&allowInsecure=1#T");
  QCOMPARE(trojan.value("type").toString(), QString("trojan"));
  QVERIFY(trojan.value("tls").toObject().value("enabled").toBool());
  QVERIFY(trojan.value("tls").toObject().value("insecure").toBool());

  const QJsonObject hy2 = SubscriptionParser::parseHysteria2URI(
      "hy2://password@hy2.example.com:8443?sni=sni.example.com&obfs=salamander&obfs-password=obfs#HY2");
  QCOMPARE(hy2.value("type").toString(), QString("hysteria2"));
  QCOMPARE(hy2.value("obfs").toObject().value("type").toString(),
           QString("salamander"));

  const QJsonObject hysteria = SubscriptionParser::parseHysteriaURI(
      "hysteria://auth@hy.example.com:443?up=10&down=20&sni=sni.example.com&allow_insecure=1&obfs=salamander&obfsParam=p#HY");
  QCOMPARE(hysteria.value("type").toString(), QString("hysteria"));
  QCOMPARE(hysteria.value("up_mbps").toString(), QString("10"));
  QCOMPARE(hysteria.value("down_mbps").toString(), QString("20"));
  QVERIFY(hysteria.value("tls").toObject().value("insecure").toBool());
  QCOMPARE(hysteria.value("obfs").toObject().value("password").toString(),
           QString("p"));

  const QJsonObject tuic = SubscriptionParser::parseTuicURI(
      "tuic://123e4567-e89b-12d3-a456-426614174000:pwd@tuic.example.com:443"
      "?token=t&congestion_control=bbr&udp_relay_mode=native&heartbeat_interval=10s"
      "&alpn=h3,h2&sni=sni.example.com&allow_insecure=1#TUIC");
  QCOMPARE(tuic.value("type").toString(), QString("tuic"));
  QCOMPARE(tuic.value("token").toString(), QString("t"));
  QCOMPARE(tuic.value("congestion_control").toString(), QString("bbr"));
  QCOMPARE(tuic.value("alpn").toArray().size(), 2);
  QVERIFY(tuic.value("tls").toObject().value("enabled").toBool());
}

void UnitUtilsTest::subscriptionParser_shouldParseClashSip008AndSingleJson() {
  const QByteArray clashYaml =
      "proxies:\n"
      "  - name: vm1\n"
      "    type: vmess\n"
      "    server: vm.example.com\n"
      "    port: 443\n"
      "    uuid: 123e4567-e89b-12d3-a456-426614174000\n"
      "    alterId: 0\n"
      "    cipher: auto\n"
      "    tls: true\n"
      "    servername: sni.example.com\n"
      "    network: ws\n"
      "    ws-opts:\n"
      "      path: /ws\n"
      "      headers:\n"
      "        Host: cdn.example.com\n"
      "  - name: vl1\n"
      "    type: vless\n"
      "    server: vless.example.com\n"
      "    port: 8443\n"
      "    uuid: 123e4567-e89b-12d3-a456-426614174000\n"
      "    flow: xtls-rprx-vision\n"
      "    network: grpc\n"
      "    grpc-opts:\n"
      "      grpc-service-name: svc\n"
      "  - name: ss1\n"
      "    type: ss\n"
      "    server: ss.example.com\n"
      "    port: 8388\n"
      "    cipher: aes-128-gcm\n"
      "    password: p\n";
  const QJsonArray clash = SubscriptionParser::parseClashConfig(clashYaml);
  QCOMPARE(clash.size(), 3);
  QCOMPARE(clash[0].toObject().value("transport").toObject().value("type").toString(),
           QString("ws"));
  QCOMPARE(clash[1].toObject().value("transport").toObject().value("type").toString(),
           QString("grpc"));
  QCOMPARE(clash[2].toObject().value("type").toString(), QString("shadowsocks"));

  QJsonObject sip008;
  sip008["servers"] = QJsonArray{
      QJsonObject{{"server", "1.2.3.4"},
                  {"server_port", 8388},
                  {"method", "aes-128-gcm"},
                  {"password", "pw"},
                  {"remarks", "node-a"}},
      QJsonObject{{"server", "5.6.7.8"},
                  {"server_port", 443},
                  {"method", "chacha20-ietf-poly1305"},
                  {"password", "pw2"},
                  {"name", "node-b"}}};
  const QJsonArray sipNodes = SubscriptionParser::parseSip008Config(sip008);
  QCOMPARE(sipNodes.size(), 2);
  QCOMPARE(sipNodes[0].toObject().value("tag").toString(), QString("node-a"));
  QCOMPARE(sipNodes[1].toObject().value("tag").toString(), QString("node-b"));

  const QJsonObject single = SubscriptionParser::parseSingleJsonNode(
      QJsonObject{{"protocol", "vmess"},
                  {"address", "host.example.com"},
                  {"port", 1234},
                  {"name", "alias"}});
  QCOMPARE(single.value("type").toString(), QString("vmess"));
  QCOMPARE(single.value("server").toString(), QString("host.example.com"));
  QCOMPARE(single.value("server_port").toInt(), 1234);
  QCOMPARE(single.value("tag").toString(), QString("alias"));
}

void UnitUtilsTest::subscriptionParser_shouldParseSingBoxAndMixedUriList() {
  const QJsonObject singBoxRoot{
      {"outbounds",
       QJsonArray{
           QJsonObject{{"type", "vmess"},
                       {"tag", "vm"},
                       {"server", "vm.example.com"},
                       {"server_port", 443}},
           QJsonObject{{"type", "direct"},
                       {"tag", "direct"},
                       {"server", "127.0.0.1"},
                       {"server_port", 0}},
       }},
      {"endpoints",
       QJsonArray{
           QJsonObject{{"type", "vless"},
                       {"tag", "vl"},
                       {"server", "vl.example.com"},
                       {"server_port", 8443}},
       }}};
  const QByteArray singJson =
      QJsonDocument(singBoxRoot).toJson(QJsonDocument::Compact);
  const QJsonArray singNodes = SubscriptionParser::parseSingBoxConfig(singJson);
  QCOMPARE(singNodes.size(), 2);

  const QJsonObject oneJsonNode{
      {"type", "socks"}, {"server", "8.8.8.8"}, {"server_port", 1080}};
  const QString jsonLine = QString("json://%1")
                               .arg(QString::fromLatin1(QJsonDocument(oneJsonNode)
                                                             .toJson(QJsonDocument::Compact)
                                                             .toBase64()));
  const QString mixedUris =
      "http://example.com:8080#H1\n"
      "socks://u:p@1.2.3.4:1080#S1\n" +
      jsonLine + "\n";
  const QJsonArray mixedNodes =
      SubscriptionParser::parseURIList(mixedUris.toUtf8());
  QVERIFY(mixedNodes.size() >= 3);
  QCOMPARE(mixedNodes[0].toObject().value("type").toString(), QString("http"));
  QCOMPARE(mixedNodes[1].toObject().value("type").toString(), QString("socks"));
}

QTEST_GUILESS_MAIN(UnitUtilsTest)

#include "unit_utils_test.moc"
