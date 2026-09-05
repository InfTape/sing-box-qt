#ifndef LOGRETENTION_H
#define LOGRETENTION_H
#include <QDateTime>
#include <QString>

namespace LogRetention {
constexpr qint64 kRetentionSeconds = 24 * 60 * 60;
QString lockFilePath(const QString& directory);
bool prune(const QString& directory,
           const QDateTime& now = QDateTime::currentDateTime());
}  // namespace LogRetention
#endif  // LOGRETENTION_H
