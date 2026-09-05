#include "core/DataUsageTracker.h"
#include <QDateTime>
#include <QCoreApplication>
#include <QJsonValue>
#include <QSqlQuery>
#include <QSqlError>
#include "utils/Logger.h"
#include <QTimer>
#include <algorithm>
#include "storage/DatabaseService.h"

namespace {
constexpr qint64 kRetryIntervalMs = 30 * 1000;

QString normalizeProcessLabel(const QString& process) {
  if (process.isEmpty()) {
    return QString();
  }
  int lastSlash = process.lastIndexOf('/');
  int lastBack  = process.lastIndexOf('\\');
  int idx       = std::max(lastSlash, lastBack);
  if (idx >= 0 && idx + 1 < process.size()) {
    return process.mid(idx + 1);
  }
  return process;
}
}  // namespace

DataUsageTracker::DataUsageTracker(QObject* parent) : QObject(parent) {
  loadFromStorage();
  auto* timer = new QTimer(this);
  timer->setInterval(kRetryIntervalMs);
  connect(timer, &QTimer::timeout, this, [this]() { flush(); });
  timer->start();
  if (auto* app = QCoreApplication::instance()) {
    connect(app, &QCoreApplication::aboutToQuit, this, [this]() { flush(); });
  }
}

DataUsageTracker::~DataUsageTracker() {
  flush();
}

void DataUsageTracker::reset() {
  if (!flush()) {
    return;
  }
  auto db = QSqlDatabase::database();
  QSqlQuery query(db);
  if (!db.transaction()) {
    return;
  }
  // Preserve active baselines so pre-reset traffic cannot return.
  if (!query.exec("DELETE FROM usage_entries") ||
      !query.exec("DELETE FROM usage_summary") || !db.commit()) {
    db.rollback();
    Logger::error("Failed to reset traffic statistics");
    return;
  }
  m_globalTotals = {};
  emit dataUsageUpdated(snapshot());
}

void DataUsageTracker::resetSession() {
  // Reconnecting to the same kernel must not count its counters twice.
  flush();
}

QString DataUsageTracker::typeKey(Type type) {
  switch (type) {
    case Type::SourceIP:
      return QStringLiteral("sourceIP");
    case Type::Host:
      return QStringLiteral("host");
    case Type::Process:
      return QStringLiteral("process");
    case Type::Outbound:
      return QStringLiteral("outbound");
  }
  return QStringLiteral("unknown");
}

QList<DataUsageTracker::Type> DataUsageTracker::allTypes() {
  return {Type::SourceIP, Type::Host, Type::Process, Type::Outbound};
}

void DataUsageTracker::updateFromConnections(const QJsonObject& connections) {
  if (!connections.value("connections").isArray()) {
    return;
  }
  // Commit the failed batch before replacing it, even if its connections have
  // since closed. Retain only this one batch while storage is unavailable.
  if (!m_pendingConnections.isEmpty() && !flush()) {
    return;
  }
  m_pendingConnections = connections;
  flush();
}

bool DataUsageTracker::flush() {
  if (m_pendingConnections.isEmpty()) {
    return true;
  }
  auto db = QSqlDatabase::database();
  if (!db.transaction()) {
    return false;
  }
  QSqlQuery entryQuery(db), counterQuery(db), deleteQuery(db);
  auto fail = [&]() {
    Logger::error("Failed to persist traffic statistics; retrying next poll: " +
                  db.lastError().text() + entryQuery.lastError().text() +
                  counterQuery.lastError().text() + deleteQuery.lastError().text());
    db.rollback();
    return false;
  };
  if (!entryQuery.prepare(
          "INSERT INTO usage_entries VALUES (?, ?, ?, ?, ?, ?, ?) "
          "ON CONFLICT(type, label) DO UPDATE SET "
          "upload=upload+excluded.upload, download=download+excluded.download, "
          "total=total+excluded.total, last_seen=excluded.last_seen") ||
      !counterQuery.prepare("INSERT INTO usage_connections VALUES (?, ?, ?) "
                            "ON CONFLICT(id) DO UPDATE SET "
                            "upload=excluded.upload, download=excluded.download") ||
      !deleteQuery.prepare("DELETE FROM usage_connections WHERE id=?")) {
    return fail();
  }
  const auto conns = m_pendingConnections.value("connections").toArray();
  QHash<QString, QPair<qint64, qint64>> nextCounters;
  auto totals = m_globalTotals;
  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  bool changed = false;
  for (const auto& item : conns) {
    const auto conn = item.toObject();
    const auto id = conn.value("id").toString();
    if (id.isEmpty() || nextCounters.contains(id)) {
      continue;
    }
    const qint64 upload =
        std::max<qint64>(0, conn.value("upload").toVariant().toLongLong());
    const qint64 download =
        std::max<qint64>(0, conn.value("download").toVariant().toLongLong());
    const auto last = m_lastById.value(id, {0, 0});
    const qint64 deltaUp = upload >= last.first ? upload - last.first : upload;
    const qint64 deltaDown =
        download >= last.second ? download - last.second : download;
    nextCounters.insert(id, {upload, download});
    if (!m_lastById.contains(id) || last != qMakePair(upload, download)) {
      counterQuery.bindValue(0, id);
      counterQuery.bindValue(1, upload);
      counterQuery.bindValue(2, download);
      if (!counterQuery.exec()) {
        return fail();
      }
    }
    if (deltaUp == 0 && deltaDown == 0) {
      continue;
    }
    changed = true;
    totals.upload += deltaUp;
    totals.download += deltaDown;
    const QJsonObject meta   = conn.value("metadata").toObject();
    QString           source = meta.value("sourceIP").toString();
    if (source.isEmpty()) {
      source = QStringLiteral("Inner");
    }
    QString host = meta.value("host").toString();
    if (host.isEmpty()) {
      host = meta.value("destinationIP").toString();
    }
    if (host.isEmpty()) {
      host = meta.value("destinationIp").toString();
    }
    if (host.isEmpty()) {
      host = QStringLiteral("Unknown");
    }
    QString process = meta.value("process").toString();
    if (process.isEmpty()) {
      process = meta.value("processName").toString();
    }
    if (process.isEmpty()) {
      process = meta.value("processPath").toString();
    }
    process = normalizeProcessLabel(process);
    if (process.isEmpty()) {
      process = QStringLiteral("Unknown");
    }
    QString          outbound;
    const QJsonArray chains = conn.value("chains").toArray();
    if (!chains.isEmpty()) {
      outbound = chains.first().toString();
    }
    if (outbound.isEmpty()) {
      outbound = conn.value("outbound").toString();
    }
    if (outbound.isEmpty()) {
      outbound = meta.value("outbound").toString();
    }
    if (outbound.isEmpty()) {
      outbound = QStringLiteral("DIRECT");
    }
    const QStringList labels{source, host, process, outbound};
    for (int type = 0; type < labels.size(); ++type) {
      entryQuery.bindValue(0, type);
      entryQuery.bindValue(1, labels[type]);
      entryQuery.bindValue(2, deltaUp);
      entryQuery.bindValue(3, deltaDown);
      entryQuery.bindValue(4, deltaUp + deltaDown);
      entryQuery.bindValue(5, nowMs);
      entryQuery.bindValue(6, nowMs);
      if (!entryQuery.exec()) {
        return fail();
      }
    }
  }
  for (auto it = m_lastById.cbegin(); it != m_lastById.cend(); ++it) {
    if (!nextCounters.contains(it.key())) {
      deleteQuery.bindValue(0, it.key());
      if (!deleteQuery.exec()) {
        return fail();
      }
    }
  }
  if (!db.commit()) {
    return fail();
  }
  m_lastById = std::move(nextCounters);
  m_globalTotals = totals;
  m_pendingConnections = {};
  if (changed) {
    emit dataUsageUpdated(snapshot());
  }
  return true;
}

QJsonObject DataUsageTracker::buildTypeSnapshot(Type type, int limit) const {
  QSqlQuery query;
  query.setForwardOnly(true);
  query.prepare("SELECT label, upload, download, total, first_seen, last_seen "
                "FROM usage_entries WHERE type=? ORDER BY total DESC, label "
                "LIMIT ?");
  query.addBindValue(static_cast<int>(type));
  query.addBindValue(std::clamp(limit, 1, 200));
  QJsonArray entries;
  const QStringList fields{"upload", "download", "total", "firstSeen", "lastSeen"};
  if (query.exec()) {
    while (query.next()) {
      QJsonObject entry{{"label", query.value(0).toString()}};
      for (int i = 0; i < fields.size(); ++i) {
        entry.insert(fields[i], query.value(i + 1).toString());
      }
      entries.append(entry);
    }
  }
  query.finish();
  query.prepare("SELECT count, upload, download, total, first_seen, last_seen "
                "FROM usage_summary WHERE type=?");
  query.addBindValue(static_cast<int>(type));
  const bool found = query.exec() && query.next();
  QJsonObject summary{{"count", found ? query.value(0).toInt() : 0}};
  for (int i = 0; i < fields.size(); ++i) {
    summary.insert(fields[i], found ? query.value(i + 1).toString() : "0");
  }
  return {{"entries", entries}, {"summary", summary}};
}

QJsonObject DataUsageTracker::snapshot(int limitPerType) const {
  QJsonObject payload;
  for (Type type : allTypes()) {
    payload.insert(typeKey(type), buildTypeSnapshot(type, limitPerType));
  }
  payload.insert("globalTotals",
                 QJsonObject{{"upload", QString::number(m_globalTotals.upload)},
                             {"download", QString::number(m_globalTotals.download)}});
  payload.insert("updatedAt", QString::number(QDateTime::currentMSecsSinceEpoch()));
  return payload;
}

DataUsageTracker::GlobalTotals DataUsageTracker::globalTotals() const {
  return m_globalTotals;
}

void DataUsageTracker::loadFromStorage() {
  if (!DatabaseService::instance().init()) {
    return;
  }
  QSqlQuery query;
  query.setForwardOnly(true);
  if (query.exec("SELECT upload, download FROM usage_summary WHERE type=0") &&
      query.next()) {
    m_globalTotals = {query.value(0).toLongLong(), query.value(1).toLongLong()};
  }
  query.finish();
  if (query.exec("SELECT id, upload, download FROM usage_connections")) {
    while (query.next()) {
      m_lastById.insert(query.value(0).toString(),
                       {query.value(1).toLongLong(), query.value(2).toLongLong()});
    }
  }
}
