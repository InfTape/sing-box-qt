#ifndef LOGPARSER_H
#define LOGPARSER_H
#include <QDateTime>
#include <QString>

namespace LogParser {
struct LogEntry {
  QString   type;
  QString   payload;
  QString   direction;
  QDateTime timestamp;
  QString   network;
};

struct LogKind {
  QString direction;
  QString host;
  QString nodeName;
  QString protocol;
  QString network;
  bool    isConnection = false;
  bool    isDns        = false;
};

LogKind parseLogKind(const QString& message);
QString logTypeLabel(const QString& type);
QString stripSessionTracker(const QString& message);
}  // namespace LogParser
#endif  // LOGPARSER_H
