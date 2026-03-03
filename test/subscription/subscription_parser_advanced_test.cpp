#include "../unit_test_shared.h"

class SubscriptionParserAdvancedTests : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void subscriptionParser_shouldParseAdvancedProtocols();
  void subscriptionParser_shouldParseClashSip008AndSingleJson();
  void subscriptionParser_shouldParseSingBoxAndMixedUriList();
};

void SubscriptionParserAdvancedTests::initTestCase() {
  (void)DatabaseService::instance().init();
}



void SubscriptionParserAdvancedTests::subscriptionParser_shouldParseAdvancedProtocols() {
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



void SubscriptionParserAdvancedTests::subscriptionParser_shouldParseClashSip008AndSingleJson() {
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



void SubscriptionParserAdvancedTests::subscriptionParser_shouldParseSingBoxAndMixedUriList() {
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



int runSubscriptionParserAdvancedTests(int argc, char* argv[]) {
  SubscriptionParserAdvancedTests tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "subscription_parser_advanced_test.moc"
