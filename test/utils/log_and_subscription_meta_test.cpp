#include "../unit_test_shared.h"

class LogAndSubscriptionMetaTests : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void logParser_shouldParseAndDetectTypes();
  void logParser_shouldHandleFallbackPaths();
  void logParser_shouldMapLabels();
  void subscriptionUserinfo_shouldParseHeader();
  void subscriptionUserinfo_shouldIgnoreInvalidSegments();
  void subscriptionFormat_shouldFormatFields();
  void subscriptionFormat_shouldHandleAdditionalRanges();
};

void LogAndSubscriptionMetaTests::initTestCase() {
  (void)DatabaseService::instance().init();
}



void LogAndSubscriptionMetaTests::logParser_shouldParseAndDetectTypes() {
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



void LogAndSubscriptionMetaTests::logParser_shouldHandleFallbackPaths() {
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
}



void LogAndSubscriptionMetaTests::logParser_shouldMapLabels() {
  QCOMPARE(LogParser::logTypeLabel("trace"), QString("TRACE"));
  QCOMPARE(LogParser::logTypeLabel("debug"), QString("DEBUG"));
  QCOMPARE(LogParser::logTypeLabel("info"), QString("INFO"));
  QCOMPARE(LogParser::logTypeLabel("warning"), QString("WARN"));
  QCOMPARE(LogParser::logTypeLabel("error"), QString("ERROR"));
  QCOMPARE(LogParser::logTypeLabel("fatal"), QString("FATAL"));
  QCOMPARE(LogParser::logTypeLabel("panic"), QString("PANIC"));
  QCOMPARE(LogParser::logTypeLabel("unknown"), QString("INFO"));
}



void LogAndSubscriptionMetaTests::subscriptionUserinfo_shouldParseHeader() {
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



void LogAndSubscriptionMetaTests::subscriptionUserinfo_shouldIgnoreInvalidSegments() {
  const QJsonObject invalid = SubscriptionUserinfo::parseUserinfoHeader(
      "upload=10=20;justtext;download=abc;expire=-1;total=5");
  QVERIFY(!invalid.contains("upload"));
  QCOMPARE(invalid.value("download").toInteger(), 0);
  QVERIFY(!invalid.contains("expire"));
  QCOMPARE(invalid.value("total").toInteger(), 5);
}



void LogAndSubscriptionMetaTests::subscriptionFormat_shouldFormatFields() {
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



void LogAndSubscriptionMetaTests::subscriptionFormat_shouldHandleAdditionalRanges() {
  QCOMPARE(SubscriptionFormat::formatBytes(1024LL * 1024), QString("1.00 MB"));
  QCOMPARE(SubscriptionFormat::formatBytes(1024LL * 1024 * 1024),
           QString("1.00 GB"));

  QVERIFY(!SubscriptionFormat::formatTimestamp(1).isEmpty());
  QCOMPARE(SubscriptionFormat::formatExpireTime(-1), QString());
}



int runLogAndSubscriptionMetaTests(int argc, char* argv[]) {
  LogAndSubscriptionMetaTests tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "log_and_subscription_meta_test.moc"
