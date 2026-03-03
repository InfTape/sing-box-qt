#include "../unit_test_shared.h"

class ConfigBuilderTests : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void configBuilder_shouldBuildFeatureEnabledBaseConfig();
  void configBuilder_shouldBuildMinimalBaseConfigWhenFeaturesDisabled();
};

void ConfigBuilderTests::initTestCase() {
  (void)DatabaseService::instance().init();
}



void ConfigBuilderTests::configBuilder_shouldBuildFeatureEnabledBaseConfig() {
  AppSettingsScopeGuard guard;
  AppSettings&          settings = AppSettings::instance();
  settings.setBlockAds(true);
  settings.setEnableAppGroups(true);
  settings.setDnsHijack(true);
  settings.setRouteSniffEnabled(true);
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
  QVERIFY(!tunInbound.contains("sniff"));
  QVERIFY(!tunInbound.contains("sniff_override_destination"));

  const QJsonObject dnsObj = config.value("dns").toObject();
  QCOMPARE(dnsObj.value("final").toString(), ConfigConstants::DNS_PROXY);
  QCOMPARE(dnsObj.value("strategy").toString(), QString("prefer_ipv6"));
  const QJsonArray dnsServers = dnsObj.value("servers").toArray();
  const QJsonObject dnsProxyServer =
      findObjectByTag(dnsServers, ConfigConstants::DNS_PROXY);
  QCOMPARE(dnsProxyServer.value("detour").toString(), ConfigConstants::TAG_AUTO);
  QVERIFY(!dnsProxyServer.contains("address"));
  QVERIFY(!dnsProxyServer.value("type").toString().isEmpty());
  QVERIFY(!findObjectByTag(dnsServers, ConfigConstants::DNS_CN).contains("detour"));
  QVERIFY(
      !findObjectByTag(dnsServers, ConfigConstants::DNS_RESOLVER).contains("detour"));
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
  QVERIFY(findActionIndex(routeRules, "sniff") >= 0);
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
  const QString cnRuleSetUrl =
      ruleSets[cnIdx].toObject().value("url").toString();
  QVERIFY(cnRuleSetUrl.startsWith(
      "https://testingcf.jsdelivr.net/gh/MetaCubeX/meta-rules-dat@sing/geo/"));


  QCOMPARE(ruleSets[cnIdx].toObject().value("download_detour").toString(),
           ConfigConstants::TAG_MANUAL);
  QCOMPARE(ruleSets[privateIdx].toObject().value("download_detour").toString(),
           ConfigConstants::TAG_MANUAL);

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



void ConfigBuilderTests::configBuilder_shouldBuildMinimalBaseConfigWhenFeaturesDisabled() {
  AppSettingsScopeGuard guard;
  AppSettings&          settings = AppSettings::instance();
  settings.setBlockAds(false);
  settings.setEnableAppGroups(false);
  settings.setDnsHijack(false);
  settings.setRouteSniffEnabled(false);
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
  QCOMPARE(findActionIndex(routeRules, "sniff"), -1);
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



int runConfigBuilderTests(int argc, char* argv[]) {
  ConfigBuilderTests tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "config_builder_test.moc"
