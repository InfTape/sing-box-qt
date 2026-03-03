#include "../unit_test_shared.h"

class FormatAndProxyTests : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void homeFormat_shouldFormatBytesAndDuration();
  void homeFormat_shouldCoverAdditionalUnits();
  void proxyNodeHelper_shouldMapDelayState();
  void proxyNodeHelper_shouldHandleAdditionalDelayForms();
};

void FormatAndProxyTests::initTestCase() {
  (void)DatabaseService::instance().init();
}



void FormatAndProxyTests::homeFormat_shouldFormatBytesAndDuration() {
  QCOMPARE(HomeFormat::bytes(0), QString("0 B"));
  QCOMPARE(HomeFormat::bytes(1), QString("1 B"));
  QCOMPARE(HomeFormat::bytes(1024), QString("1.00 KB"));
  QCOMPARE(HomeFormat::bytes(1536), QString("1.50 KB"));

  QCOMPARE(HomeFormat::duration(59), QString("00:59"));
  QCOMPARE(HomeFormat::duration(3600), QString("1:00:00"));
  QCOMPARE(HomeFormat::duration(3661), QString("1:01:01"));
}



void FormatAndProxyTests::homeFormat_shouldCoverAdditionalUnits() {
  QCOMPARE(HomeFormat::bytes(1048576), QString("1.00 MB"));
  QCOMPARE(HomeFormat::bytes(1073741824), QString("1.00 GB"));
  QCOMPARE(HomeFormat::bytes(1099511627776), QString("1.00 TB"));

  QCOMPARE(HomeFormat::duration(0), QString("00:00"));
  QCOMPARE(HomeFormat::duration(61), QString("01:01"));
}



void FormatAndProxyTests::proxyNodeHelper_shouldMapDelayState() {
  QCOMPARE(ProxyNodeHelper::delayStateFromText(""), QString("loading"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("..."), QString("loading"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("90 ms"), QString("ok"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("120 ms"), QString("warn"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("300 ms"), QString("bad"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("-1"), QString("bad"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("not-a-number"), QString(""));
}



void FormatAndProxyTests::proxyNodeHelper_shouldHandleAdditionalDelayForms() {
  QCOMPARE(ProxyNodeHelper::delayStateFromText("0 ms"), QString("bad"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("100 ms"), QString("warn"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("299 ms"), QString("warn"));
  QCOMPARE(ProxyNodeHelper::delayStateFromText("250"), QString("warn"));
}



int runFormatAndProxyTests(int argc, char* argv[]) {
  FormatAndProxyTests tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "format_and_proxy_test.moc"
