#ifndef LOGSTORE_H
#define LOGSTORE_H
#include <QObject>
#include <QFuture>
#include <QSqlDatabase>
#include <QVector>
#include "utils/LogParser.h"

// A separate, disposable database: retention never touches lifetime traffic.
class LogStore : public QObject {
  Q_OBJECT
 public:
  static constexpr int kVisibleLimit = 50;
  struct Row {
    qint64 id = 0;
    LogParser::LogEntry entry;
  };
  struct Counts {
    qint64 total = 0;
    qint64 errors = 0;
    qint64 warnings = 0;
  };

  explicit LogStore(QObject* parent = nullptr, const QString& path = {});
  ~LogStore() override;
  bool append(const LogParser::LogEntry& entry);
  bool flush();
  bool clear();
  bool prune(const QDateTime& now = QDateTime::currentDateTime());
  QVector<Row> latest(const QString& search, const QString& type,
                      qint64 beforeId = 0);
  Counts counts(const QString& search, const QString& type);
  bool exportTo(const QString& path, const QString& search, const QString& type);
  QFuture<QString> exportAsync(const QString& path, const QString& search,
                               const QString& type);
  QString error() const { return m_error; }
 signals:
  void changed();
  void storageError(const QString& message);
 private:
  bool open(const QString& path);
  bool fail(const QString& message);
  QSqlDatabase m_db;
  QString m_connectionName;
  QString m_error;
  QVector<LogParser::LogEntry> m_pending;
  qsizetype m_pendingBytes = 0;
};
#endif  // LOGSTORE_H
