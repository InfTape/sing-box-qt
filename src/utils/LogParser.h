#ifndef LOGPARSER_H
#define LOGPARSER_H

#include <QDateTime>
#include <QString>

namespace LogParser {

struct LogEntry {
    QString type;
    QString payload;
    QString direction;
    QDateTime timestamp;
};

struct LogKind {
    QString direction;
    QString host;
    QString nodeName;
    QString protocol;
    bool isConnection = false;
    bool isDns = false;
};

QString stripAnsiSequences(const QString &text);
LogKind parseLogKind(const QString &message);
QString detectLogType(const QString &message);
QString logTypeLabel(const QString &type);

} // namespace LogParser

#endif // LOGPARSER_H
