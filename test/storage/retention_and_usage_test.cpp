#include "../unit_test_shared.h"
#include <QLockFile>
#include <QSqlQuery>
#include <QTimer>
#include <memory>
#include "usage_test_helpers.h"
#include "storage/LogStore.h"
#include "utils/LogRetention.h"
#include "utils/Logger.h"

namespace {
QJsonObject usageConnection(const QString& id, qint64 up, qint64 down,
                            const QString& host = "example.com") {
  return {{"id", id}, {"upload", up}, {"download", down},
          {"metadata", QJsonObject{{"sourceIP", "127.0.0.1"},
                                    {"host", host}, {"process", "browser"}}},
          {"outbound", "proxy"}};
}

QJsonObject usageSnapshot(std::initializer_list<QJsonObject> connections) {
  QJsonArray array;
  for (const auto& connection : connections) array.append(connection);
  return {{"connections", array}};
}

bool writeLog(const QString& path, const QByteArray& contents) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

QByteArray readLog(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  return file.readAll();
}
}  // namespace

class RetentionAndUsageTests : public QObject {
  Q_OBJECT
 private slots:
  void init() {
    m_database = std::make_unique<UsageDatabaseScope>();
    QVERIFY(m_database->directory.isValid());
    QVERIFY(DatabaseService::instance().init());
  }

  void cleanup() {
    m_database.reset();
  }

  void persistsOnExitAndResumesConnectionDeltas() {
    {
      DataUsageTracker tracker;
      tracker.updateFromConnections(usageSnapshot({usageConnection("a", 100, 200)}));
      QCOMPARE(tracker.globalTotals().upload, 100);
    }
    DataUsageTracker restored;
    QCOMPARE(restored.globalTotals().download, 200);
    restored.resetSession();
    restored.updateFromConnections(usageSnapshot({usageConnection("a", 130, 260),
                                                   usageConnection("b", 10, 20)}));
    QCOMPARE(restored.globalTotals().upload, 140);
    QCOMPARE(restored.globalTotals().download, 280);
    restored.resetSession();
    restored.updateFromConnections(usageSnapshot({usageConnection("a", 130, 260),
                                                   usageConnection("b", 10, 20)}));
    QCOMPARE(restored.globalTotals().upload, 140);
    // A fresh kernel uses new IDs. Its first traffic is counted in full.
    restored.updateFromConnections(usageSnapshot({usageConnection("new", 50, 60)}));
    QCOMPARE(restored.globalTotals().upload, 190);
    QCOMPARE(restored.globalTotals().download, 340);
  }

  void manualResetDoesNotRecountActiveConnections() {
    {
      DataUsageTracker tracker;
      tracker.updateFromConnections(usageSnapshot({usageConnection("a", 100, 200)}));
      tracker.reset();
      QCOMPARE(tracker.globalTotals().upload, 0);
      tracker.updateFromConnections(usageSnapshot({usageConnection("a", 100, 200)}));
      QCOMPARE(tracker.globalTotals().download, 0);
    }
    DataUsageTracker restored;
    QCOMPARE(restored.globalTotals().upload, 0);
    restored.updateFromConnections(usageSnapshot({usageConnection("a", 110, 230)}));
    QCOMPARE(restored.globalTotals().upload, 10);
    QCOMPARE(restored.globalTotals().download, 30);
    restored.updateFromConnections(usageSnapshot({}));
    QCOMPARE(restored.globalTotals().upload, 10);
    restored.updateFromConnections(usageSnapshot({usageConnection("b", 20, 40)}));
    QCOMPARE(restored.globalTotals().upload, 30);
    QCOMPARE(restored.globalTotals().download, 70);
  }

  void startsFreshWithoutImportingPreviousStatistics() {
    auto& db = DatabaseService::instance();
    struct OldUsageGuard {
      QString saved;
      ~OldUsageGuard() {
        DatabaseService::instance().setValue("data_usage_v1", saved);
      }
    } guard{db.getValue("data_usage_v1", "{}")};
    const QJsonObject oldEntry{{"upload", "100"}, {"download", "200"},
                               {"total", "300"}, {"firstSeen", "1000"},
                               {"lastSeen", "2000"}};
    const QJsonObject oldUsage{
        {"outbound", QJsonObject{{"proxy", oldEntry}}},
        {"host", QJsonObject{{"example.com", oldEntry}}}};
    const QString oldJson = QString::fromUtf8(
        QJsonDocument(oldUsage).toJson(QJsonDocument::Compact));
    QVERIFY(db.setValue("data_usage_v1", oldJson));

    DataUsageTracker tracker;
    QCOMPARE(tracker.globalTotals().upload, 0);
    QCOMPARE(tracker.globalTotals().download, 0);
    tracker.updateFromConnections(usageSnapshot({usageConnection("a", 10, 20)}));
    QCOMPARE(tracker.globalTotals().upload, 10);
    QCOMPARE(tracker.globalTotals().download, 20);
    QVERIFY(tracker.flush());
    QCOMPARE(tracker.snapshot()["host"].toObject()["entries"].toArray().size(), 1);
    QCOMPARE(db.getValue("data_usage_v1"), oldJson);
  }

  void retainsRankingsBeyondOldEntryLimit() {
    {
      DataUsageTracker tracker;
      QJsonArray entries;
      for (int i = 0; i < 5001; ++i) {
        entries.append(usageConnection(QString::number(i), 1, 2,
                                         QString("host-%1.test").arg(i)));
      }
      tracker.updateFromConnections({{"connections", entries}});
      QCOMPARE(tracker.snapshot()["host"].toObject()["summary"].toObject()
                   ["count"].toInt(), 5001);
      QCOMPARE(tracker.snapshot()["host"].toObject()["entries"].toArray().size(), 50);
      // Old labels stay on disk even when no connections remain in memory.
      tracker.updateFromConnections(usageSnapshot({}));
      QSqlQuery query;
      QVERIFY(query.exec("SELECT COUNT(*) FROM usage_connections"));
      QVERIFY(query.next());
      QCOMPARE(query.value(0).toInt(), 0);
      QVERIFY(query.exec("EXPLAIN QUERY PLAN SELECT label FROM usage_entries "
                         "WHERE type=1 ORDER BY total DESC, label LIMIT 50"));
      QVERIFY(query.next());
      QVERIFY(query.value(3).toString().contains("usage_ranking"));
    }
    DataUsageTracker restored;
    QCOMPARE(restored.globalTotals().upload, 5001);
    QCOMPARE(restored.snapshot()["host"].toObject()["summary"].toObject()
                 ["count"].toInt(), 5001);
  }

  void ignoresInvalidSnapshotsAndHandlesCounterRestart() {
    DataUsageTracker tracker;
    tracker.updateFromConnections(usageSnapshot({usageConnection("a", 100, 200)}));
    tracker.updateFromConnections({{"error", "temporarily unavailable"}});
    tracker.updateFromConnections(usageSnapshot({usageConnection("a", 110, 220)}));
    QCOMPARE(tracker.globalTotals().upload, 110);
    tracker.updateFromConnections(usageSnapshot({usageConnection("a", 5, 10)}));
    QCOMPARE(tracker.globalTotals().upload, 115);
    QCOMPARE(tracker.globalTotals().download, 230);
  }

  void timerFlushesAndFailedWritesCanRetry() {
    DataUsageTracker tracker;
    tracker.updateFromConnections(usageSnapshot({usageConnection("a", 100, 200)}));
    auto* timer = tracker.findChild<QTimer*>();
    QVERIFY(timer);
    QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
    QCOMPARE(DataUsageTracker().snapshot()["globalTotals"]
                 .toObject()["upload"].toString(), QString("100"));
    struct WriteGuard {
      ~WriteGuard() { QSqlQuery().exec("PRAGMA query_only = OFF"); }
    } guard;
    QVERIFY(QSqlQuery().exec("PRAGMA query_only = ON"));
    tracker.updateFromConnections(usageSnapshot({usageConnection("a", 150, 240)}));
    QVERIFY(!tracker.flush());
    QCOMPARE(tracker.globalTotals().upload, 100);
    tracker.reset();
    QCOMPARE(tracker.globalTotals().upload, 100);
    QVERIFY(QSqlQuery().exec("PRAGMA query_only = OFF"));
    QVERIFY(tracker.flush());
    QCOMPARE(DataUsageTracker().snapshot()["globalTotals"]
                 .toObject()["upload"].toString(), QString("150"));
  }

  void totalsSurviveMoreThan24HoursAndKeepRankingOrder() {
    DataUsageTracker tracker;
    tracker.updateFromConnections(usageSnapshot({usageConnection("a", 100, 200)}));
    QSqlQuery query;
    const auto old = QDateTime::currentDateTime().addDays(-30).toMSecsSinceEpoch();
    QVERIFY(query.exec(QString("UPDATE usage_entries SET first_seen=%1, last_seen=%1")
                           .arg(old)));
    DataUsageTracker restarted;
    QCOMPARE(restarted.globalTotals().upload, 100);
    restarted.updateFromConnections(usageSnapshot({usageConnection("b", 1000, 2000,
                                                                   "top.test")}));
    QCOMPARE(restarted.globalTotals().upload, 1100);
    const auto entries = restarted.snapshot()["host"].toObject()["entries"].toArray();
    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries.first().toObject()["label"].toString(), QString("top.test"));
    QCOMPARE(restarted.snapshot()["host"].toObject()["summary"].toObject()
                 ["count"].toInt(), 2);
  }

  void diskLogHistorySupportsBoundedSearchExportAndExpiry() {
    QTemporaryDir directory;
    const auto path = directory.filePath("history.db");
    const auto now = QDateTime::currentDateTime();
    {
      LogStore store(nullptr, path);
      for (int i = 0; i < 450; ++i) {
        QVERIFY(store.append({i % 2 ? "error" : "info",
                              QString("message-%1-end").arg(i), {}, now}));
      }
      QVERIFY(store.append({"warning", QString::fromUtf8("\xC3\x84rger 100% _ test"), {}, now}));
      QVERIFY(store.append({"info", "expired", {}, now.addDays(-2)}));
      QVERIFY(store.flush());
      QCOMPARE(store.counts({}, {}).total, 451);
      const auto latest = store.latest({}, {});
      QCOMPARE(latest.size(), 50);
      QCOMPARE(latest.first().entry.payload, QString("message-401-end"));
      QCOMPARE(latest.last().entry.type, QString("warning"));
      QCOMPARE(store.latest("message-0-end", {}).size(), 1);
      QCOMPARE(store.latest(QString::fromUtf8("\xC3\xA4rger 100% _"), {}).size(), 1);
      QCOMPARE(store.counts({}, "error").total, 225);
      const auto exportPath = directory.filePath("all.txt");
      QVERIFY(store.exportTo(exportPath, "message-", {}));
      const auto exported = readLog(exportPath);
      QCOMPARE(exported.count('\n'), 450);
      QVERIFY(exported.contains("message-0-end"));
      QVERIFY(exported.contains("message-449-end"));
      QVERIFY(!exported.contains("expired"));
      auto future = store.exportAsync(directory.filePath("errors.txt"), {}, "error");
      // New arrivals must not enter an export that has already been requested.
      QVERIFY(store.append({"error", "after export started", {}, now}));
      QVERIFY(store.flush());
      future.waitForFinished();
      QVERIFY2(future.result().isEmpty(), qPrintable(future.result()));
      QCOMPARE(readLog(directory.filePath("errors.txt")).count('\n'), 225);
      QVERIFY(store.prune(now));
    }
    {
      LogStore restored(nullptr, path);
      QCOMPARE(restored.counts({}, {}).total, 452);
      QVERIFY(restored.prune(now.addDays(2)));
      QCOMPARE(restored.counts({}, {}).total, 0);
      QVERIFY(restored.latest({}, {}).isEmpty());
      QVERIFY(restored.append({"info", "new", {}, now}));
      QVERIFY(restored.clear());
      QVERIFY(restored.flush());
      QCOMPARE(restored.counts({}, {}).total, 0);
    }
  }

  void retriesClosedConnectionsBeforeReplacingAFailedBatch() {
    DataUsageTracker tracker;
    tracker.updateFromConnections(usageSnapshot({usageConnection("a", 100, 200)}));
    struct WriteGuard {
      ~WriteGuard() { QSqlQuery().exec("PRAGMA query_only = OFF"); }
    } guard;
    QVERIFY(QSqlQuery().exec("PRAGMA query_only = ON"));
    tracker.updateFromConnections(usageSnapshot({usageConnection("a", 150, 240)}));
    QVERIFY(QSqlQuery().exec("PRAGMA query_only = OFF"));
    tracker.updateFromConnections(usageSnapshot({}));
    QCOMPARE(tracker.globalTotals().upload, 150);
    QCOMPARE(tracker.globalTotals().download, 240);
    QSqlQuery query;
    QVERIFY(query.exec("SELECT COUNT(*) FROM usage_connections"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0);
  }

  void failedLogWritesKeepOnlyABoundedBatchAndCanRetry() {
    QTemporaryDir directory;
    const auto path = directory.filePath("history.db");
    LogStore store(nullptr, path);
    QString name;
    for (const auto& candidate : QSqlDatabase::connectionNames()) {
      if (QSqlDatabase::database(candidate).databaseName() == path) name = candidate;
    }
    QVERIFY(!name.isEmpty());
    QSqlQuery query(QSqlDatabase::database(name));
    QVERIFY(query.exec("PRAGMA query_only = ON"));
    QSignalSpy errors(&store, &LogStore::storageError);
    for (int i = 0; i < 120; ++i) {
      store.append({"info", QString("buffered-%1").arg(i), {},
                    QDateTime::currentDateTime()});
    }
    QVERIFY(!errors.isEmpty());
    QVERIFY(query.exec("PRAGMA query_only = OFF"));
    QVERIFY(store.flush());
    QCOMPARE(store.counts({}, {}).total, 100);
    QVERIFY(store.append({"info", "after recovery", {}, QDateTime::currentDateTime()}));
    QVERIFY(store.flush());
    QCOMPARE(store.counts({}, {}).total, 101);
  }

  void retentionKeepsOnlyLast24HoursAndMultilineMessages() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto now = QDateTime::fromString("2026-09-05T12:00:00", Qt::ISODate);
    const QByteArray old = "[2026-09-04 11:59:59.999] [INFO] expired\nold detail\n";
    const QByteArray kept = "[2026-09-04 12:00:00.000] [ERROR] keep\nkeep detail\n"
                            "[2026-09-04 23:59:59.000] [INFO] yesterday night\n";
    const QString yesterday = directory.filePath("2026-09-04.log");
    QVERIFY(writeLog(yesterday, old + kept));
    // Old logger versions kept appending after midnight to an old filename.
    const QString legacy = directory.filePath("2026-09-01.log");
    const QByteArray legacyKept = "[2026-09-05 11:00:00.000] [INFO] recent\n";
    QVERIFY(writeLog(legacy, "[2026-09-01 10:00:00.000] [INFO] old\n" + legacyKept));
    const QString expired = directory.filePath("2026-09-03.log");
    QVERIFY(writeLog(expired, "[2026-09-03 10:00:00.000] [INFO] expired\n"));
    const QString unrelated = directory.filePath("manual-export.log");
    QVERIFY(writeLog(unrelated, "leave alone"));
    DataUsageTracker tracker;
    tracker.updateFromConnections(usageSnapshot({usageConnection("a", 100, 200)}));
    QVERIFY(tracker.flush());
    const QJsonObject savedStats =
        tracker.snapshot().value("globalTotals").toObject();
    QVERIFY(LogRetention::prune(directory.path(), now));
    QCOMPARE(readLog(yesterday), kept);
    QCOMPARE(readLog(legacy), legacyKept);
    QVERIFY(!QFile::exists(expired));
    QCOMPARE(readLog(unrelated), QByteArray("leave alone"));
    DataUsageTracker restoredTracker;
    QCOMPARE(restoredTracker.snapshot().value("globalTotals").toObject(),
             savedStats);
    QVERIFY(LogRetention::prune(directory.path(), now.addSecs(24 * 3600)));
    QVERIFY(!QFile::exists(yesterday));
    QVERIFY(!QFile::exists(legacy));
  }

  void retentionHonorsWriterLock() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString expired = directory.filePath("2026-09-01.log");
    const QByteArray contents = "[2026-09-01 10:00:00.000] [INFO] old\n";
    QVERIFY(writeLog(expired, contents));
    QLockFile lock(LogRetention::lockFilePath(directory.path()));
    QVERIFY(lock.tryLock());
    const auto now = QDateTime::fromString("2026-09-05T12:00:00", Qt::ISODate);
    QVERIFY(!LogRetention::prune(directory.path(), now));
    QCOMPARE(readLog(expired), contents);
    lock.unlock();
    QVERIFY(LogRetention::prune(directory.path(), now));
    QVERIFY(!QFile::exists(expired));
  }

  void retentionExpiresMinuteSegmentsAtBoundary() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto now = QDateTime::fromString("2026-09-05T12:00:30", Qt::ISODate);
    const QString old = directory.filePath("2026-09-04-11-59.log");
    const QString boundary = directory.filePath("2026-09-04-12-00.log");
    const QString recent = directory.filePath("2026-09-04-12-01.log");
    QVERIFY(writeLog(old, "[2026-09-04 11:59:00.000] [INFO] old\n"));
    const QByteArray kept = "[2026-09-04 12:00:30.000] [INFO] keep\n";
    QVERIFY(writeLog(boundary,
                     "[2026-09-04 12:00:29.999] [INFO] expired\n" + kept));
    QVERIFY(writeLog(recent, "[2026-09-04 12:01:00.000] [INFO] recent\n"));
    QVERIFY(LogRetention::prune(directory.path(), now));
    QVERIFY(!QFile::exists(old));
    QCOMPARE(readLog(boundary), kept);
    QVERIFY(QFile::exists(recent));
  }

  void loggerWritesSegmentsAndCleansUpWhileIdle() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    struct EnvironmentGuard {
      QByteArray dataDir = qgetenv("SING_BOX_QT_DATA_DIR");
      bool wasSet = qEnvironmentVariableIsSet("SING_BOX_QT_DATA_DIR");
      ~EnvironmentGuard() {
        Logger::instance().close();
        if (wasSet) qputenv("SING_BOX_QT_DATA_DIR", dataDir);
        else qunsetenv("SING_BOX_QT_DATA_DIR");
      }
    } guard;
    Logger::instance().close();
    qputenv("SING_BOX_QT_DATA_DIR", directory.path().toUtf8());
    const QDir logs(directory.filePath("logs"));
    QVERIFY(QDir().mkpath(logs.path()));
    const auto oldTime = QDateTime::currentDateTime().addDays(-2);
    const QString oldPath = logs.filePath(oldTime.toString("yyyy-MM-dd") + ".log");
    const QByteArray oldLine =
        ("[" + oldTime.toString("yyyy-MM-dd hh:mm:ss.zzz") + "] [INFO] old\n")
            .toUtf8();
    QVERIFY(writeLog(oldPath, oldLine));
    Logger::instance().init();
    QVERIFY(!QFile::exists(oldPath));
    Logger::info("retention-test-marker");
    QByteArray logContents;
    for (const auto& name : logs.entryList({"*.log"}, QDir::Files)) {
      logContents += readLog(logs.filePath(name));
      QVERIFY(QDateTime::fromString(QFileInfo(name).completeBaseName(),
                                    "yyyy-MM-dd-HH-mm").isValid());
    }
    QVERIFY(logContents.contains("retention-test-marker"));
    QVERIFY(writeLog(oldPath, oldLine));
    auto* timer = QCoreApplication::instance()->findChild<QTimer*>(
        "LogRetentionTimer");
    QVERIFY(timer);
    QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
    QVERIFY(!QFile::exists(oldPath));
  }

 private:
  std::unique_ptr<UsageDatabaseScope> m_database;
};

int runRetentionAndUsageTests(int argc, char* argv[]) {
  RetentionAndUsageTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "retention_and_usage_test.moc"
