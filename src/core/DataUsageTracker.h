#ifndef DATAUSAGETRACKER_H
#define DATAUSAGETRACKER_H

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QVector>

class DataUsageTracker : public QObject {
  Q_OBJECT
 public:
  enum class Type { SourceIP = 0, Host = 1, Process = 2, Outbound = 3 };

  explicit DataUsageTracker(QObject* parent = nullptr);

  void        reset();
  void        resetSession();
  void        updateFromConnections(const QJsonObject& connections);
  QJsonObject snapshot(int limitPerType = 50) const;

 signals:
  void dataUsageUpdated(const QJsonObject& snapshot);

 private:
  struct Entry {
    QString label;
    qint64  upload      = 0;
    qint64  download    = 0;
    qint64  total       = 0;
    qint64  firstSeenMs = 0;
    qint64  lastSeenMs  = 0;
  };

  struct Totals {
    int    count       = 0;
    qint64 upload      = 0;
    qint64 download    = 0;
    qint64 total       = 0;
    qint64 firstSeenMs = 0;
    qint64 lastSeenMs  = 0;
  };

  static QString     typeKey(Type type);
  static QList<Type> allTypes();

  void              addDelta(Type type, const QString& label, qint64 upload,
                             qint64 download, qint64 nowMs);
  void              loadFromStorage();
  void              persistToStorage() const;
  void              restoreFromPayload(const QJsonObject& payload);
  QJsonObject       buildPersistPayload() const;
  QVector<Entry>    sortedEntries(const QHash<QString, Entry>& map,
                                  int limit) const;
  Totals            buildTotals(const QHash<QString, Entry>& map) const;
  QJsonObject       buildTypeSnapshot(Type type, int limit) const;

  QHash<QString, Entry>                 m_entries[4];
  QHash<QString, QPair<qint64, qint64>> m_lastById;
  bool                                 m_initialized       = false;
  bool                                 m_loadedFromStorage = false;
};

#endif  // DATAUSAGETRACKER_H
