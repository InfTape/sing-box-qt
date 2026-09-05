#ifndef DATAUSAGETRACKER_H
#define DATAUSAGETRACKER_H
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

class DataUsageTracker : public QObject {
  Q_OBJECT
 public:
  enum class Type { SourceIP = 0, Host = 1, Process = 2, Outbound = 3 };

  struct GlobalTotals {
    qint64 upload   = 0;
    qint64 download = 0;
  };

  explicit DataUsageTracker(QObject* parent = nullptr);
  ~DataUsageTracker() override;
  void         reset();
  void         resetSession();
  bool         flush();
  void         updateFromConnections(const QJsonObject& connections);
  QJsonObject  snapshot(int limitPerType = 50) const;
  GlobalTotals globalTotals() const;
 signals:
  void dataUsageUpdated(const QJsonObject& snapshot);

 private:
  static QString typeKey(Type type);
  static QList<Type> allTypes();
  void loadFromStorage();
  QJsonObject buildTypeSnapshot(Type type, int limit) const;
  QHash<QString, QPair<qint64, qint64>> m_lastById;
  GlobalTotals m_globalTotals;
  // At most one active-connections response is retained for a failed write.
  QJsonObject m_pendingConnections;
};
#endif  // DATAUSAGETRACKER_H
