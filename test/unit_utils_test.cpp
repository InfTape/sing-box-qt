#include <QObject>
#include <QRegularExpression>
#include <QtTest/QtTest>
#include "utils/LogParser.h"
#include "utils/home/HomeFormat.h"
#include "utils/proxy/ProxyNodeHelper.h"
#include "utils/rule/RuleUtils.h"
#include "utils/subscription/SubscriptionFormat.h"
#include "utils/subscription/SubscriptionUserinfo.h"

class UnitUtilsTest : public QObject {
  Q_OBJECT

 private slots:
  void ruleUtils_shouldNormalizeTypeAndProxy();
  void ruleUtils_shouldDetectCustomPayload();
  void homeFormat_shouldFormatBytesAndDuration();
  void proxyNodeHelper_shouldMapDelayState();
  void logParser_shouldParseAndDetectTypes();
  void subscriptionUserinfo_shouldParseHeader();
  void subscriptionFormat_shouldFormatFields();
};

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
  QVERIFY(!RuleUtils::isCustomPayload("geoip-cn"));
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

void UnitUtilsTest::proxyNodeHelper_shouldMapDelayState() {
  QCOMPARE(ProxyNodeHelper::delayStateFromText(""), QString("loading"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("..."), QString("loading"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("90 ms"), QString("ok"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("120 ms"), QString("warn"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("300 ms"), QString("bad"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("-1"), QString("bad"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("not-a-number"), QString(""));
}

void UnitUtilsTest::logParser_shouldParseAndDetectTypes() {
  QCOMPARE(LogParser::stripAnsiSequences("\x1B[31mERROR\x1B[0m test"),
           QString("ERROR test"));

  QCOMPARE(LogParser::detectLogType("panic happened"), QString("panic"));
  QCOMPARE(LogParser::detectLogType("FATAL: fail"), QString("fatal"));
  QCOMPARE(LogParser::detectLogType("error happened"), QString("error"));
  QCOMPARE(LogParser::detectLogType("warning message"), QString("warning"));
  QCOMPARE(LogParser::detectLogType("debug trace"), QString("debug"));
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

QTEST_APPLESS_MAIN(UnitUtilsTest)

#include "unit_utils_test.moc"
