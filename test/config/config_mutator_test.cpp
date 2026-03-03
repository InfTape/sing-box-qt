#include "../unit_test_shared.h"

class ConfigMutatorTests : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void configMutator_shouldUpdateAndReadDefaultMode();
  void configMutator_shouldApplySharedRules();
  void configMutator_shouldInjectNodesAndUpdateSelectors();
  void configMutator_shouldApplyPortSettingsAndFeatureRemovals();
  void configMutator_shouldApplySettingsFeatureInsertions();
};

void ConfigMutatorTests::initTestCase() {
  (void)DatabaseService::instance().init();
}



void ConfigMutatorTests::configMutator_shouldUpdateAndReadDefaultMode() {
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



void ConfigMutatorTests::configMutator_shouldApplySharedRules() {
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



void ConfigMutatorTests::configMutator_shouldInjectNodesAndUpdateSelectors() {
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



void ConfigMutatorTests::configMutator_shouldApplyPortSettingsAndFeatureRemovals() {
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
  settings.setDnsCn("223.5.5.5");
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
                       {"type", "https"},
                       {"server", "old-proxy.example"},
                       {"path", "/dns-query"},
                       {"detour", "old"}},
           QJsonObject{{"tag", ConfigConstants::DNS_CN},
                       {"type", "udp"},
                       {"server", "old-cn"}},
           QJsonObject{{"tag", ConfigConstants::DNS_RESOLVER},
                       {"type", "udp"},
                       {"server", "old-resolver"}},
        }},
      {"rules",
       QJsonArray{
           QJsonObject{{"rule_set", ConfigConstants::RS_GEOSITE_ADS},
                       {"action", "predefined"},
                       {"rcode", "NOERROR"}},
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
  const QJsonObject dnsProxyServer =
      findObjectByTag(dnsServers, ConfigConstants::DNS_PROXY);
  QCOMPARE(dnsProxyServer.value("type").toString(), QString("https"));
  QCOMPARE(dnsProxyServer.value("server").toString(), QString("1.0.0.1"));
  QCOMPARE(dnsProxyServer.value("path").toString(), QString("/dns-query"));
  QCOMPARE(dnsProxyServer.value("detour").toString(), ConfigConstants::TAG_MANUAL);
  QVERIFY(!dnsProxyServer.contains("address"));
  QCOMPARE(findObjectByTag(dnsServers, ConfigConstants::DNS_CN)
               .value("server")
               .toString(),
           QString("223.5.5.5"));
  QCOMPARE(findObjectByTag(dnsServers, ConfigConstants::DNS_RESOLVER)
               .value("server")
               .toString(),
           QString("223.5.5.5"));
  QVERIFY(!findObjectByTag(dnsServers, ConfigConstants::DNS_CN).contains("detour"));
  QVERIFY(
      !findObjectByTag(dnsServers, ConfigConstants::DNS_RESOLVER).contains("detour"));

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



void ConfigMutatorTests::configMutator_shouldApplySettingsFeatureInsertions() {
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
                              {"url", "https://raw.githubusercontent.com/old"},
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
  QVERIFY(findActionIndex(routeRules, "sniff") >= 0);
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
  const QString cnRuleSetUrl =
      ruleSets[cnIdx].toObject().value("url").toString();
  QVERIFY(cnRuleSetUrl.startsWith(
      "https://testingcf.jsdelivr.net/gh/MetaCubeX/meta-rules-dat@sing/geo/"));

}



int runConfigMutatorTests(int argc, char* argv[]) {
  ConfigMutatorTests tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "config_mutator_test.moc"
