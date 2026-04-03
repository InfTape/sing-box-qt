#include "../unit_test_shared.h"
#include "app/interfaces/ConfigRepository.h"

namespace {
class FakeConfigRepository : public ConfigRepository {
 public:
  QString getConfigDir() const override { return QStringLiteral("memory"); }

  QString getActiveConfigPath() const override { return m_activeConfigPath; }

  bool generateConfigWithNodes(const QJsonArray& nodes,
                               const QString&    targetPath) override {
    QJsonObject config;
    config["outbounds"] = nodes;
    return saveConfig(targetPath, config);
  }

  bool updateClashDefaultMode(const QString&,
                              const QString&,
                              QString*) override {
    return true;
  }

  QString readClashDefaultMode(const QString&) const override { return {}; }

  QJsonObject loadConfig(const QString& path) override {
    return m_configs.value(path);
  }

  bool saveConfig(const QString& path, const QJsonObject& config) override {
    if (path.trimmed().isEmpty()) {
      return false;
    }
    m_configs.insert(path, config);
    if (m_activeConfigPath.isEmpty()) {
      m_activeConfigPath = path;
    }
    return true;
  }

  void applySettingsToConfig(QJsonObject&) override {}

  void applyPortSettings(QJsonObject&) override {}

  int mixedPort() const override { return 2080; }

 private:
  QString                    m_activeConfigPath;
  QHash<QString, QJsonObject> m_configs;
};

struct DatabaseStateGuard {
  DatabaseService&       db = DatabaseService::instance();
  QJsonArray             subscriptions;
  int                    activeIndex = -1;
  QString                activeConfigPath;
  QHash<QString, QJsonArray> subscriptionNodes;

  explicit DatabaseStateGuard(const QStringList& ids)
      : subscriptions(db.getSubscriptions()),
        activeIndex(db.getActiveSubscriptionIndex()),
        activeConfigPath(db.getActiveConfigPath()) {
    for (const auto& id : ids) {
      subscriptionNodes.insert(id, db.getSubscriptionNodes(id));
    }
  }

  ~DatabaseStateGuard() {
    db.saveSubscriptions(subscriptions);
    db.saveActiveSubscriptionIndex(activeIndex);
    db.saveActiveConfigPath(activeConfigPath);
    for (auto it = subscriptionNodes.begin(); it != subscriptionNodes.end();
         ++it) {
      db.saveSubscriptionNodes(it.key(), it.value());
    }
  }
};

QJsonObject makeNodeOutbound(const QString& tag, const QString& server) {
  QJsonObject node;
  node["type"]        = "shadowsocks";
  node["tag"]         = tag;
  node["server"]      = server;
  node["server_port"] = 443;
  return node;
}

QJsonObject makeSelectorOutbound(const QString&     tag,
                                 const QStringList& members) {
  QJsonObject selector;
  selector["type"]      = "selector";
  selector["tag"]       = tag;
  selector["outbounds"] = QJsonArray::fromStringList(members);
  return selector;
}

QJsonObject makeUrltestOutbound(const QString&     tag,
                                const QStringList& members) {
  QJsonObject outbound;
  outbound["type"]      = "urltest";
  outbound["tag"]       = tag;
  outbound["outbounds"] = QJsonArray::fromStringList(members);
  return outbound;
}

QJsonObject makeSubscriptionRecord(const QString& id,
                                   const QString& name,
                                   const QString& configPath) {
  QJsonObject record;
  record["id"]         = id;
  record["name"]       = name;
  record["config_path"] = configPath;
  record["enabled"]    = true;
  record["is_manual"]  = false;
  record["rule_sets"]  = QJsonArray{QStringLiteral("default")};
  return record;
}

QJsonObject buildActiveConfig(const QStringList& manualMembers,
                              const QJsonArray&  extraOutbounds = {}) {
  QJsonArray outbounds;
  outbounds.append(
      makeUrltestOutbound(ConfigConstants::TAG_AUTO, {ConfigConstants::TAG_DIRECT}));
  outbounds.append(
      makeSelectorOutbound(ConfigConstants::TAG_MANUAL, manualMembers));
  outbounds.append(makeNodeOutbound(QStringLiteral("node-a"),
                                    QStringLiteral("active-a.example.com")));
  for (const auto& outbound : extraOutbounds) {
    outbounds.append(outbound);
  }
  outbounds.append(QJsonObject{{"type", "direct"},
                               {"tag", ConfigConstants::TAG_DIRECT}});
  outbounds.append(QJsonObject{{"type", "block"},
                               {"tag", ConfigConstants::TAG_BLOCK}});

  QJsonObject config;
  config["outbounds"] = outbounds;
  return config;
}
}  // namespace

class SubscriptionServiceTests : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void addSubscriptionNodesToActiveGroup_shouldInsertImportedSelectorAfterAuto();
  void addSubscriptionNodesToActiveGroup_shouldMoveExistingImportedSelectorAfterAuto();
};

void SubscriptionServiceTests::initTestCase() {
  QVERIFY(DatabaseService::instance().init());
}

void SubscriptionServiceTests::
    addSubscriptionNodesToActiveGroup_shouldInsertImportedSelectorAfterAuto() {
  const QString activeId = QStringLiteral("test-active-sub");
  const QString sourceId = QStringLiteral("test-source-sub");
  DatabaseStateGuard guard({activeId, sourceId});

  FakeConfigRepository repo;
  const QString        activeConfigPath = QStringLiteral("memory://active.json");
  QVERIFY(repo.saveConfig(
      activeConfigPath,
      buildActiveConfig({ConfigConstants::TAG_AUTO, QStringLiteral("node-a")})));

  QJsonArray subscriptions;
  subscriptions.append(makeSubscriptionRecord(
      activeId, QStringLiteral("Active Subscription"), activeConfigPath));
  subscriptions.append(makeSubscriptionRecord(
      sourceId, QStringLiteral("Imported Group"), QString()));
  QVERIFY(DatabaseService::instance().saveSubscriptions(subscriptions));
  QVERIFY(DatabaseService::instance().saveActiveSubscriptionIndex(-1));
  QVERIFY(DatabaseService::instance().saveActiveConfigPath(QString()));
  QVERIFY(DatabaseService::instance().saveSubscriptionNodes(
      sourceId,
      QJsonArray{
          makeNodeOutbound(QStringLiteral("remote-1"),
                           QStringLiteral("remote-1.example.com"))}));

  SubscriptionService service(&repo);
  service.setActiveSubscription(activeId, false);

  QString error;
  QVERIFY2(service.addSubscriptionNodesToActiveGroup(sourceId, &error),
           qPrintable(error));

  const QJsonArray outbounds =
      repo.loadConfig(activeConfigPath).value("outbounds").toArray();
  const QJsonArray manualMembers =
      findObjectByTag(outbounds, ConfigConstants::TAG_MANUAL)
          .value("outbounds")
          .toArray();
  QCOMPARE(manualMembers.size(), 3);
  QCOMPARE(manualMembers[0].toString(), ConfigConstants::TAG_AUTO);
  QCOMPARE(manualMembers[1].toString(), QStringLiteral("Imported Group"));
  QCOMPARE(manualMembers[2].toString(), QStringLiteral("node-a"));

  const QJsonArray importedMembers =
      findObjectByTag(outbounds, QStringLiteral("Imported Group"))
          .value("outbounds")
          .toArray();
  QCOMPARE(importedMembers.size(), 1);
  QCOMPARE(importedMembers[0].toString(), QStringLiteral("remote-1"));
}

void SubscriptionServiceTests::
    addSubscriptionNodesToActiveGroup_shouldMoveExistingImportedSelectorAfterAuto() {
  const QString activeId = QStringLiteral("test-active-sub");
  const QString sourceId = QStringLiteral("test-source-sub");
  DatabaseStateGuard guard({activeId, sourceId});

  FakeConfigRepository repo;
  const QString        activeConfigPath = QStringLiteral("memory://active.json");
  QJsonArray           extraOutbounds;
  extraOutbounds.append(
      makeSelectorOutbound(QStringLiteral("Imported Group"),
                           {QStringLiteral("remote-old")}));
  QVERIFY(repo.saveConfig(
      activeConfigPath,
      buildActiveConfig({ConfigConstants::TAG_AUTO,
                         QStringLiteral("node-a"),
                         QStringLiteral("Imported Group")},
                        extraOutbounds)));

  QJsonArray subscriptions;
  subscriptions.append(makeSubscriptionRecord(
      activeId, QStringLiteral("Active Subscription"), activeConfigPath));
  subscriptions.append(makeSubscriptionRecord(
      sourceId, QStringLiteral("Imported Group"), QString()));
  QVERIFY(DatabaseService::instance().saveSubscriptions(subscriptions));
  QVERIFY(DatabaseService::instance().saveActiveSubscriptionIndex(-1));
  QVERIFY(DatabaseService::instance().saveActiveConfigPath(QString()));
  QVERIFY(DatabaseService::instance().saveSubscriptionNodes(
      sourceId,
      QJsonArray{
          makeNodeOutbound(QStringLiteral("remote-2"),
                           QStringLiteral("remote-2.example.com"))}));

  SubscriptionService service(&repo);
  service.setActiveSubscription(activeId, false);

  QString error;
  QVERIFY2(service.addSubscriptionNodesToActiveGroup(sourceId, &error),
           qPrintable(error));

  const QJsonArray outbounds =
      repo.loadConfig(activeConfigPath).value("outbounds").toArray();
  const QJsonArray manualMembers =
      findObjectByTag(outbounds, ConfigConstants::TAG_MANUAL)
          .value("outbounds")
          .toArray();
  QCOMPARE(manualMembers.size(), 3);
  QCOMPARE(manualMembers[0].toString(), ConfigConstants::TAG_AUTO);
  QCOMPARE(manualMembers[1].toString(), QStringLiteral("Imported Group"));
  QCOMPARE(manualMembers[2].toString(), QStringLiteral("node-a"));

  const QJsonArray importedMembers =
      findObjectByTag(outbounds, QStringLiteral("Imported Group"))
          .value("outbounds")
          .toArray();
  QCOMPARE(importedMembers.size(), 2);
  QCOMPARE(importedMembers[0].toString(), QStringLiteral("remote-old"));
  QCOMPARE(importedMembers[1].toString(), QStringLiteral("remote-2"));
}

int runSubscriptionServiceTests(int argc, char* argv[]) {
  SubscriptionServiceTests tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "subscription_service_test.moc"
