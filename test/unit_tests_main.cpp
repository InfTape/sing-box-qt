#include <QCoreApplication>
#include <QtTest/QtTest>

namespace {
struct TestSuite {
  const char* name;
  int (*run)(int argc, char* argv[]);
};
}  // namespace

int runConfigBuilderTests(int argc, char* argv[]);
int runConfigMutatorTests(int argc, char* argv[]);
int runSubscriptionParserBasicTests(int argc, char* argv[]);
int runSubscriptionParserAdvancedTests(int argc, char* argv[]);
int runSubscriptionServiceTests(int argc, char* argv[]);
int runRuleAndRuleConfigTests(int argc, char* argv[]);
int runFormatAndProxyTests(int argc, char* argv[]);
int runLogAndSubscriptionMetaTests(int argc, char* argv[]);
int runMiscServicesTests(int argc, char* argv[]);

int main(int argc, char* argv[]) {
  qputenv("SING_BOX_QT_PORTABLE", "1");
  QCoreApplication app(argc, argv);

  const TestSuite suites[] = {
      {"config-builder", runConfigBuilderTests},
      {"config-mutator", runConfigMutatorTests},
      {"subscription-parser-basic", runSubscriptionParserBasicTests},
      {"subscription-parser-advanced", runSubscriptionParserAdvancedTests},
      {"subscription-service", runSubscriptionServiceTests},
      {"rule-and-ruleconfig", runRuleAndRuleConfigTests},
      {"format-and-proxy", runFormatAndProxyTests},
      {"log-and-subscription-meta", runLogAndSubscriptionMetaTests},
      {"misc-services", runMiscServicesTests},
  };

  const QStringList args = app.arguments();
  const int suiteArgIndex = args.indexOf("--suite");
  if (suiteArgIndex >= 0) {
    if (suiteArgIndex + 1 >= args.size()) {
      qCritical("Missing value for --suite.");
      return 1;
    }
    const QString suiteName = args.at(suiteArgIndex + 1);
    for (const auto& suite : suites) {
      if (suiteName == QLatin1String(suite.name)) {
        QStringList qtestArgs = args;
        qtestArgs.removeAt(suiteArgIndex + 1);
        qtestArgs.removeAt(suiteArgIndex);

        QVector<QByteArray> encodedArgs;
        QVector<char*>      argvCopy;
        encodedArgs.reserve(qtestArgs.size());
        argvCopy.reserve(qtestArgs.size());
        for (const QString& arg : qtestArgs) {
          encodedArgs.append(arg.toLocal8Bit());
          argvCopy.append(encodedArgs.last().data());
        }
        return suite.run(argvCopy.size(), argvCopy.data());
      }
    }
    qCritical("Unknown test suite: %s", qPrintable(suiteName));
    return 1;
  }

  int status = 0;
  for (const auto& suite : suites) {
    status |= suite.run(argc, argv);
  }
  return status;
}
