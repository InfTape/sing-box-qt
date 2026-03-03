#include "../unit_test_shared.h"

class SubscriptionParserBasicTests : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void subscriptionParser_shouldParseCommonUris();
  void subscriptionParser_shouldParseWireguardConfig();
  void subscriptionParser_shouldHandleBase64Fallbacks();
};

void SubscriptionParserBasicTests::initTestCase() {
  (void)DatabaseService::instance().init();
}



void SubscriptionParserBasicTests::subscriptionParser_shouldParseCommonUris() {
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



void SubscriptionParserBasicTests::subscriptionParser_shouldParseWireguardConfig() {
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



void SubscriptionParserBasicTests::subscriptionParser_shouldHandleBase64Fallbacks() {
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



int runSubscriptionParserBasicTests(int argc, char* argv[]) {
  SubscriptionParserBasicTests tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "subscription_parser_basic_test.moc"
