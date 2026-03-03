#include "../unit_test_shared.h"

class RuleAndRuleConfigTests : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void ruleUtils_shouldNormalizeTypeAndProxy();
  void ruleUtils_shouldHandleAdditionalProxyShapes();
  void ruleConfigService_shouldParsePayloadValues();
};

void RuleAndRuleConfigTests::initTestCase() {
  (void)DatabaseService::instance().init();
}



void RuleAndRuleConfigTests::ruleUtils_shouldNormalizeTypeAndProxy() {
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





void RuleAndRuleConfigTests::ruleUtils_shouldHandleAdditionalProxyShapes() {
  QCOMPARE(RuleUtils::normalizeProxyValue("[route(node-c)]"), QString("node-c"));
  QCOMPARE(RuleUtils::normalizeProxyValue("Proxy(Node-D)"), QString("Node-D"));
  QCOMPARE(RuleUtils::normalizeProxyValue("[Node-E]"), QString("Node-E"));
  QCOMPARE(RuleUtils::displayProxyLabel("Node-F"), QString("Node-F"));
}



void RuleAndRuleConfigTests::ruleConfigService_shouldParsePayloadValues() {
  QString     key;
  QStringList values;
  QString     error;

  QVERIFY(RuleConfigService::parseRulePayload(
      "domain_suffix=googleapis.com,gstatic.com", &key, &values, &error));
  QCOMPARE(key, QString("domain_suffix"));
  QCOMPARE(values, (QStringList{"googleapis.com", "gstatic.com"}));

  QVERIFY(RuleConfigService::parseRulePayload(
      "domain_suffix=[googleapis.com gstatic.com]", &key, &values, &error));
  QCOMPARE(key, QString("domain_suffix"));
  QCOMPARE(values, (QStringList{"googleapis.com", "gstatic.com"}));

  QVERIFY(RuleConfigService::parseRulePayload(
      "domain_suffix=[googleapis.com, gstatic.com]", &key, &values, &error));
  QCOMPARE(values, (QStringList{"googleapis.com", "gstatic.com"}));

  QVERIFY(RuleConfigService::parseRulePayload(
      "process_path=[\"C:\\Program Files\\App\\app.exe\" D:\\Tools\\tool.exe]",
      &key,
      &values,
      &error));
  QCOMPARE(key, QString("process_path"));
  QCOMPARE(values,
           (QStringList{"C:\\Program Files\\App\\app.exe",
                        "D:\\Tools\\tool.exe"}));

  QVERIFY(!RuleConfigService::parseRulePayload("domain_suffix", &key, &values, &error));
}



int runRuleAndRuleConfigTests(int argc, char* argv[]) {
  RuleAndRuleConfigTests tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "rule_and_ruleconfig_test.moc"
