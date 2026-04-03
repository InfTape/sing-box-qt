#include <QCoreApplication>
#include <QtTest/QtTest>

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
  QCoreApplication app(argc, argv);

  int status = 0;
  status |= runConfigBuilderTests(argc, argv);
  status |= runConfigMutatorTests(argc, argv);
  status |= runSubscriptionParserBasicTests(argc, argv);
  status |= runSubscriptionParserAdvancedTests(argc, argv);
  status |= runSubscriptionServiceTests(argc, argv);
  status |= runRuleAndRuleConfigTests(argc, argv);
  status |= runFormatAndProxyTests(argc, argv);
  status |= runLogAndSubscriptionMetaTests(argc, argv);
  status |= runMiscServicesTests(argc, argv);
  return status;
}
