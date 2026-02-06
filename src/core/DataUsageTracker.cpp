#include "core/DataUsageTracker.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonValue>
#include <QSet>
#include <algorithm>

#include "storage/DatabaseService.h"

namespace {
QString normalizeProcessLabel(const QString& process) {
  if (process.isEmpty()) return QString();
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
}

void DataUsageTracker::reset() {
  for (Type type : allTypes()) {
    m_entries[static_cast<int>(type)].clear();
  }
  m_lastById.clear();
  m_initialized       = false;
  m_loadedFromStorage = false;
  persistToStorage();
  emit dataUsageUpdated(snapshot());
}

void DataUsageTracker::resetSession() {
  m_lastById.clear();
  m_initialized = false;
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

void DataUsageTracker::addDelta(Type type, const QString& label, qint64 upload, qint64 download, qint64 nowMs) {
  if (label.isEmpty()) return;

  auto& map  = m_entries[static_cast<int>(type)];
  auto  iter = map.find(label);
  if (iter == map.end()) {
    Entry entry;
    entry.label       = label;
    entry.upload      = upload;
    entry.download    = download;
    entry.total       = upload + download;
    entry.firstSeenMs = nowMs;
    entry.lastSeenMs  = nowMs;
    map.insert(label, entry);
    return;
  }

  Entry& entry = iter.value();
  entry.upload += upload;
  entry.download += download;
  entry.total = entry.upload + entry.download;
  if (entry.firstSeenMs <= 0) {
    entry.firstSeenMs = nowMs;
  }
  entry.lastSeenMs = nowMs;
}

void DataUsageTracker::updateFromConnections(const QJsonObject& connections) {
  const QJsonArray conns = connections.value("connections").toArray();
  QSet<QString>    activeIds;
  const qint64     nowMs        = QDateTime::currentMSecsSinceEpoch();
  const bool       skipBaseline = (!m_initialized && m_loadedFromStorage);
  bool             changed      = false;

  for (const QJsonValue& item : conns) {
    const QJsonObject conn = item.toObject();
    const QString     id   = conn.value("id").toString();
    if (id.isEmpty()) continue;

    activeIds.insert(id);

    const qint64 upload   = conn.value("upload").toVariant().toLongLong();
    const qint64 download = conn.value("download").toVariant().toLongLong();

    const bool hasLast   = m_lastById.contains(id);
    const auto last      = m_lastById.value(id, {0, 0});
    qint64     deltaUp   = 0;
    qint64     deltaDown = 0;
    if (hasLast) {
      deltaUp   = std::max<qint64>(0, upload - last.first);
      deltaDown = std::max<qint64>(0, download - last.second);
    } else if (!skipBaseline) {
      deltaUp   = upload;
      deltaDown = download;
    }

    m_lastById[id] = {upload, download};

    if (deltaUp == 0 && deltaDown == 0) continue;
    changed = true;

    const QJsonObject meta = conn.value("metadata").toObject();

    QString source = meta.value("sourceIP").toString();
    if (source.isEmpty()) source = QStringLiteral("Inner");

    QString host = meta.value("host").toString();
    if (host.isEmpty()) host = meta.value("destinationIP").toString();
    if (host.isEmpty()) host = meta.value("destinationIp").toString();
    if (host.isEmpty()) host = QStringLiteral("Unknown");

    QString process = meta.value("process").toString();
    if (process.isEmpty()) process = meta.value("processName").toString();
    if (process.isEmpty()) process = meta.value("processPath").toString();
    process = normalizeProcessLabel(process);
    if (process.isEmpty()) process = QStringLiteral("Unknown");

    QString          outbound;
    const QJsonArray chains = conn.value("chains").toArray();
    if (!chains.isEmpty()) outbound = chains.first().toString();
    if (outbound.isEmpty()) outbound = conn.value("outbound").toString();
    if (outbound.isEmpty()) outbound = meta.value("outbound").toString();
    if (outbound.isEmpty()) outbound = QStringLiteral("DIRECT");

    addDelta(Type::SourceIP, source, deltaUp, deltaDown, nowMs);
    addDelta(Type::Host, host, deltaUp, deltaDown, nowMs);
    addDelta(Type::Process, process, deltaUp, deltaDown, nowMs);
    addDelta(Type::Outbound, outbound, deltaUp, deltaDown, nowMs);
  }

  if (activeIds.isEmpty()) {
    m_lastById.clear();
  } else {
    for (auto it = m_lastById.begin(); it != m_lastById.end();) {
      if (!activeIds.contains(it.key())) {
        it = m_lastById.erase(it);
      } else {
        ++it;
      }
    }
  }

  m_initialized       = true;
  m_loadedFromStorage = false;
  if (changed) {
    persistToStorage();
  }
  emit dataUsageUpdated(snapshot());
}

QVector<DataUsageTracker::Entry> DataUsageTracker::sortedEntries(const QHash<QString, Entry>& map, int limit) const {
  QVector<Entry> entries;
  entries.reserve(map.size());
  for (const auto& entry : map) {
    entries.push_back(entry);
  }

  std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
    if (a.total == b.total) return a.label < b.label;
    return a.total > b.total;
  });

  if (limit > 0 && entries.size() > limit) {
    entries.resize(limit);
  }
  return entries;
}

DataUsageTracker::Totals DataUsageTracker::buildTotals(const QHash<QString, Entry>& map) const {
  Totals totals;
  totals.count = map.size();
  if (map.isEmpty()) return totals;

  bool hasTime = false;
  for (const auto& entry : map) {
    totals.upload += entry.upload;
    totals.download += entry.download;
    totals.total += entry.total;

    if (entry.firstSeenMs > 0) {
      if (!hasTime || entry.firstSeenMs < totals.firstSeenMs) {
        totals.firstSeenMs = entry.firstSeenMs;
      }
      hasTime = true;
    }
    if (entry.lastSeenMs > totals.lastSeenMs) {
      totals.lastSeenMs = entry.lastSeenMs;
    }
  }

  return totals;
}

QJsonObject DataUsageTracker::buildTypeSnapshot(Type type, int limit) const {
  const auto& map    = m_entries[static_cast<int>(type)];
  const auto  list   = sortedEntries(map, limit);
  const auto  totals = buildTotals(map);

  QJsonArray entries;
  for (const auto& entry : list) {
    QJsonObject obj;
    obj.insert("label", entry.label);
    obj.insert("upload", QString::number(entry.upload));
    obj.insert("download", QString::number(entry.download));
    obj.insert("total", QString::number(entry.total));
    obj.insert("firstSeen", QString::number(entry.firstSeenMs));
    obj.insert("lastSeen", QString::number(entry.lastSeenMs));
    entries.append(obj);
  }

  QJsonObject summary;
  summary.insert("count", totals.count);
  summary.insert("upload", QString::number(totals.upload));
  summary.insert("download", QString::number(totals.download));
  summary.insert("total", QString::number(totals.total));
  summary.insert("firstSeen", QString::number(totals.firstSeenMs));
  summary.insert("lastSeen", QString::number(totals.lastSeenMs));

  QJsonObject payload;
  payload.insert("entries", entries);
  payload.insert("summary", summary);
  return payload;
}

QJsonObject DataUsageTracker::snapshot(int limitPerType) const {
  QJsonObject payload;
  for (Type type : allTypes()) {
    payload.insert(typeKey(type), buildTypeSnapshot(type, limitPerType));
  }
  payload.insert("updatedAt", QString::number(QDateTime::currentMSecsSinceEpoch()));
  return payload;
}

void DataUsageTracker::loadFromStorage() {
  QJsonObject payload = DatabaseService::instance().getDataUsage();
  restoreFromPayload(payload);
}

void DataUsageTracker::persistToStorage() const {
  DatabaseService::instance().saveDataUsage(buildPersistPayload());
}

QJsonObject DataUsageTracker::buildPersistPayload() const {
  QJsonObject root;
  for (Type type : allTypes()) {
    QJsonObject typeObj;
    const auto& map = m_entries[static_cast<int>(type)];
    for (auto it = map.begin(); it != map.end(); ++it) {
      const Entry& entry = it.value();
      QJsonObject  obj;
      obj.insert("upload", QString::number(entry.upload));
      obj.insert("download", QString::number(entry.download));
      obj.insert("total", QString::number(entry.total));
      obj.insert("firstSeen", QString::number(entry.firstSeenMs));
      obj.insert("lastSeen", QString::number(entry.lastSeenMs));
      typeObj.insert(it.key(), obj);
    }
    root.insert(typeKey(type), typeObj);
  }
  root.insert("updatedAt", QString::number(QDateTime::currentMSecsSinceEpoch()));
  return root;
}

void DataUsageTracker::restoreFromPayload(const QJsonObject& payload) {
  auto readLongLong = [](const QJsonValue& value) -> qint64 {
    if (value.isString()) {
      return value.toString().toLongLong();
    }
    return value.toVariant().toLongLong();
  };
  bool hasData = false;
  for (Type type : allTypes()) {
    QHash<QString, Entry>& map = m_entries[static_cast<int>(type)];
    map.clear();
    const QJsonObject typeObj = payload.value(typeKey(type)).toObject();
    for (auto it = typeObj.begin(); it != typeObj.end(); ++it) {
      const QString     label = it.key();
      const QJsonObject obj   = it.value().toObject();
      Entry             entry;
      entry.label       = label;
      entry.upload      = readLongLong(obj.value("upload"));
      entry.download    = readLongLong(obj.value("download"));
      entry.total       = readLongLong(obj.value("total"));
      entry.firstSeenMs = readLongLong(obj.value("firstSeen"));
      entry.lastSeenMs  = readLongLong(obj.value("lastSeen"));
      if (!label.isEmpty()) {
        map.insert(label, entry);
        hasData = true;
      }
    }
  }

  m_loadedFromStorage = hasData;
  m_initialized       = false;
}
