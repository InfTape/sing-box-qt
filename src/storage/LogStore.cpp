#include "LogStore.h"
#include <QDir>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QTimer>
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <utility>
#include "utils/AppPaths.h"
#include "utils/LogRetention.h"
#include "utils/Logger.h"

namespace {
constexpr qsizetype kBatchBytes = 256 * 1024;
constexpr qsizetype kBatchRows = 100;

QString predicate(const QString& search, const QString& type) {
  QString result = "stamp >= :cutoff";
  if (!search.isEmpty()) {
    result += " AND instr(search_text, :search) > 0";
  }
  if (!type.isEmpty()) {
    result += " AND type = :type";
  }
  return result;
}

void bindFilter(QSqlQuery& query, const QString& search, const QString& type) {
  query.bindValue(":cutoff", QDateTime::currentDateTime()
                                 .addSecs(-LogRetention::kRetentionSeconds)
                                 .toMSecsSinceEpoch());
  if (!search.isEmpty()) {
    query.bindValue(":search", search.toCaseFolded());
  }
  if (!type.isEmpty()) {
    query.bindValue(":type", type);
  }
}

LogStore::Row readRow(const QSqlQuery& query) {
  return {query.value(0).toLongLong(),
          {query.value(2).toString(), query.value(3).toString(),
           query.value(4).toString(),
           QDateTime::fromMSecsSinceEpoch(query.value(1).toLongLong())}};
}

// Returns an error string, or an empty string after an atomic successful save.
QString exportRows(QSqlDatabase& db, const QString& path, const QString& search,
                   const QString& type, qint64 lastId) {
  if (QFileInfo(path).absoluteFilePath() ==
      QFileInfo(db.databaseName()).absoluteFilePath()) {
    return QObject::tr("Choose a different file from the log database.");
  }
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return file.errorString();
  }
  QSqlQuery query(db);
  query.setForwardOnly(true);
  query.prepare("SELECT id, stamp, type, payload, direction FROM logs WHERE " +
                predicate(search, type) + " AND id <= :last ORDER BY id");
  bindFilter(query, search, type);
  query.bindValue(":last", lastId);
  if (!query.exec()) {
    return query.lastError().text();
  }
  QTextStream out(&file);
  while (query.next()) {
    const auto log = readRow(query).entry;
    out << '[' << log.timestamp.toString(Qt::ISODate) << "] ["
        << log.type.toUpper() << "] " << log.payload << '\n';
  }
  if (query.lastError().isValid()) {
    return query.lastError().text();
  }
  out.flush();
  if (out.status() != QTextStream::Ok) {
    return QObject::tr("Could not write export.");
  }
  if (!file.commit()) {
    return file.errorString();
  }
  return {};
}
}  // namespace

LogStore::LogStore(QObject* parent, const QString& path) : QObject(parent) {
  const QString filePath = path.isEmpty()
                               ? QDir(appDataDir()).filePath("logs/history.db")
                               : path;
  if (open(filePath)) {
    prune();
  }
  auto* writeTimer = new QTimer(this);
  writeTimer->setInterval(250);
  connect(writeTimer, &QTimer::timeout, this, [this]() { flush(); });
  writeTimer->start();
  auto* retentionTimer = new QTimer(this);
  retentionTimer->setObjectName("HistoryRetentionTimer");
  retentionTimer->setInterval(60 * 1000);
  connect(retentionTimer, &QTimer::timeout, this, [this]() {
    if (prune()) {
      emit changed();
    }
  });
  retentionTimer->start();
}

LogStore::~LogStore() {
  flush();
  m_db.close();
  m_db = {};
  QSqlDatabase::removeDatabase(m_connectionName);
}

bool LogStore::fail(const QString& message) {
  if (m_error != message) {
    Logger::error("Log history storage: " + message);
  }
  m_error = message;
  emit storageError(message);
  return false;
}

bool LogStore::open(const QString& path) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  m_connectionName = "log-history-" + QUuid::createUuid().toString();
  m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
  m_db.setDatabaseName(path);
  if (!m_db.open()) {
    return fail(m_db.lastError().text());
  }
  QSqlQuery query(m_db);
  const QStringList schema{
      "PRAGMA auto_vacuum = INCREMENTAL",
      "PRAGMA journal_mode = WAL",
      "PRAGMA synchronous = NORMAL",
      "PRAGMA cache_size = -2048",
      "PRAGMA temp_store = FILE",
      "PRAGMA journal_size_limit = 4194304",
      "CREATE TABLE IF NOT EXISTS logs (id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "stamp INTEGER NOT NULL, type TEXT NOT NULL, payload TEXT NOT NULL, "
      "direction TEXT NOT NULL, search_text TEXT NOT NULL)",
      "CREATE INDEX IF NOT EXISTS log_time ON logs(stamp)",
      "CREATE INDEX IF NOT EXISTS log_type_id ON logs(type, id)",
      "CREATE INDEX IF NOT EXISTS log_type_time ON logs(type, stamp)",
      "CREATE TABLE IF NOT EXISTS log_counts (type TEXT PRIMARY KEY, "
      "count INTEGER NOT NULL) WITHOUT ROWID",
      "CREATE TRIGGER IF NOT EXISTS log_insert AFTER INSERT ON logs BEGIN "
      "INSERT INTO log_counts VALUES (NEW.type, 1) ON CONFLICT(type) DO UPDATE "
      "SET count=count+1; END",
      "CREATE TRIGGER IF NOT EXISTS log_delete AFTER DELETE ON logs BEGIN "
      "UPDATE log_counts SET count=count-1 WHERE type=OLD.type; END"};
  for (const auto& sql : schema) {
    if (!query.exec(sql)) {
      return fail(query.lastError().text());
    }
  }
  return true;
}

bool LogStore::append(const LogParser::LogEntry& entry) {
  const qsizetype bytes = (entry.payload.size() * 2 + entry.type.size() +
                          entry.direction.size()) * sizeof(QChar);
  if (!m_pending.isEmpty() &&
      (m_pending.size() >= kBatchRows || m_pendingBytes + bytes > kBatchBytes) &&
      !flush()) {
    return fail(tr("Log storage is unavailable; new logs are being dropped."));
  }
  m_pending.append(entry);
  m_pendingBytes += bytes;
  if (m_pending.size() >= kBatchRows || m_pendingBytes >= kBatchBytes) {
    return flush();
  }
  return true;
}

bool LogStore::flush() {
  if (m_pending.isEmpty()) {
    return true;
  }
  if (!m_db.transaction()) {
    return fail(m_db.lastError().text());
  }
  QSqlQuery query(m_db);
  query.prepare("INSERT INTO logs(stamp, type, payload, direction, search_text) "
                "VALUES (?, ?, ?, ?, ?)");
  for (const auto& entry : std::as_const(m_pending)) {
    query.bindValue(0, entry.timestamp.toMSecsSinceEpoch());
    query.bindValue(1, entry.type.isNull() ? QStringLiteral("") : entry.type);
    query.bindValue(2, entry.payload.isNull() ? QStringLiteral("") : entry.payload);
    query.bindValue(3, entry.direction.isNull() ? QStringLiteral("") : entry.direction);
    query.bindValue(4, entry.payload.isNull() ? QStringLiteral("")
                                            : entry.payload.toCaseFolded());
    if (!query.exec()) {
      m_db.rollback();
      return fail(query.lastError().text());
    }
  }
  if (!m_db.commit()) {
    m_db.rollback();
    return fail(m_db.lastError().text());
  }
  m_pending.clear();
  m_pendingBytes = 0;
  m_error.clear();
  emit changed();
  return true;
}

bool LogStore::clear() {
  QSqlQuery query(m_db);
  if (!query.exec("DELETE FROM logs")) {
    return fail(query.lastError().text());
  }
  m_pending.clear();
  m_pendingBytes = 0;
  m_error.clear();
  query.exec("PRAGMA incremental_vacuum(256)");
  emit changed();
  return true;
}

bool LogStore::prune(const QDateTime& now) {
  if (!flush()) {
    return false;
  }
  QSqlQuery query(m_db);
  query.prepare("DELETE FROM logs WHERE stamp < ?");
  query.addBindValue(now.addSecs(-LogRetention::kRetentionSeconds)
                        .toMSecsSinceEpoch());
  if (!query.exec()) {
    return fail(query.lastError().text());
  }
  query.finish();
  // Reuse free pages immediately and gradually return them to the filesystem.
  query.exec("PRAGMA incremental_vacuum(256)");
  query.exec("PRAGMA wal_checkpoint(PASSIVE)");
  return true;
}

QVector<LogStore::Row> LogStore::latest(const QString& search,
                                      const QString& type) {
  QVector<Row> result;
  QSqlQuery query(m_db);
  query.setForwardOnly(true);
  query.prepare("SELECT id, stamp, type, payload, direction FROM logs WHERE " +
                predicate(search, type) + " ORDER BY id DESC LIMIT " +
                QString::number(kVisibleLimit));
  bindFilter(query, search, type);
  if (!query.exec()) {
    fail(query.lastError().text());
    return result;
  }
  while (query.next()) {
    result.append(readRow(query));
  }
  std::reverse(result.begin(), result.end());
  return result;
}

LogStore::Counts LogStore::counts(const QString& search, const QString& type) {
  Counts result;
  QSqlQuery query(m_db);
  query.setForwardOnly(true);
  if (search.isEmpty()) {
    // Subtract entries that expired since the last maintenance tick.
    query.prepare("SELECT type, count - (SELECT COUNT(*) FROM logs "
                  "WHERE logs.type=log_counts.type AND stamp < :cutoff) "
                  "FROM log_counts" +
                  (type.isEmpty() ? QString() : " WHERE type=:type"));
  } else {
    query.prepare("SELECT type, COUNT(*) FROM logs WHERE " +
                  predicate(search, type) + " GROUP BY type");
  }
  bindFilter(query, search, type);
  if (!query.exec()) {
    fail(query.lastError().text());
    return result;
  }
  while (query.next()) {
    const auto level = query.value(0).toString();
    const qint64 count = query.value(1).toLongLong();
    result.total += count;
    if (level == "error" || level == "fatal" || level == "panic") {
      result.errors += count;
    }
    if (level == "warning") {
      result.warnings += count;
    }
  }
  return result;
}

bool LogStore::exportTo(const QString& path, const QString& search,
                         const QString& type) {
  if (!flush()) {
    return false;
  }
  QSqlQuery query(m_db);
  if (!query.exec("SELECT MAX(id) FROM logs") || !query.next()) {
    return fail(query.lastError().text());
  }
  const qint64 lastId = query.value(0).toLongLong();
  query.finish();
  const auto error = exportRows(m_db, path, search, type, lastId);
  if (!error.isEmpty()) {
    return fail(error);
  }
  return true;
}

QFuture<QString> LogStore::exportAsync(const QString& path, const QString& search,
                                      const QString& type) {
  QString error;
  qint64 lastId = 0;
  if (!flush()) {
    error = m_error;
  }
  else {
    QSqlQuery query(m_db);
    if (query.exec("SELECT MAX(id) FROM logs") && query.next()) {
      lastId = query.value(0).toLongLong();
    } else error = query.lastError().text();
  }
  const QString databasePath = m_db.databaseName();
  return QtConcurrent::run([databasePath, path, search, type, lastId, error]() {
    if (!error.isEmpty()) {
      return error;
    }
    const auto name = "log-export-" + QUuid::createUuid().toString();
    QString result;
    {
      auto db = QSqlDatabase::addDatabase("QSQLITE", name);
      db.setDatabaseName(databasePath);
      db.setConnectOptions("QSQLITE_OPEN_READONLY");
      if (!db.open()) {
        result = db.lastError().text();
      }
      else {
        QSqlQuery query(db);
        query.exec("PRAGMA cache_size = -2048");
        query.exec("PRAGMA temp_store = FILE");
        result = exportRows(db, path, search, type, lastId);
      }
    }
    QSqlDatabase::removeDatabase(name);
    return result;
  });
}
