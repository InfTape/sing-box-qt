#include "LogRetention.h"
#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QSaveFile>

namespace LogRetention {
QString lockFilePath(const QString& directory) {
  return QDir(directory).filePath("writer.lock");
}

bool prune(const QString& directory, const QDateTime& now) {
  const QDir dir(directory);
  if (!dir.exists() || !now.isValid()) {
    return false;
  }
  // The UI and core manager can share this directory. Never trim a file while
  // another process is appending to it.
  QLockFile lock(lockFilePath(directory));
  if (!lock.tryLock(1000)) {
    return false;
  }
  const QDateTime cutoff = now.addSecs(-kRetentionSeconds);
  bool success = true;
  for (const auto& file : dir.entryInfoList({"*.log"},
                                           QDir::Files | QDir::NoSymLinks)) {
    const QString name = file.completeBaseName();
    const auto minute = QDateTime::fromString(name, "yyyy-MM-dd-HH-mm");
    QDateTime start;
    if (minute.isValid() && minute.toString("yyyy-MM-dd-HH-mm") == name) {
      if (minute >= cutoff) {
        continue;
      }
      if (minute.addSecs(60) <= cutoff) {
        success = QFile::remove(file.absoluteFilePath()) && success;
        continue;
      }
      start = minute;
    } else {
      const QDate date = QDate::fromString(name, "yyyy-MM-dd");
      if (!date.isValid() || date.toString("yyyy-MM-dd") != name ||
          date > cutoff.date()) {
        continue;
      }
      start = date.startOfDay();
    }
    QFile input(file.absoluteFilePath());
    QSaveFile output(file.absoluteFilePath());
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
      success = false;
      continue;
    }
    bool keep = start >= cutoff;
    bool removed = false;
    qint64 retainedBytes = 0;
    while (!input.atEnd()) {
      const QByteArray line = input.readLine();
      if (line.size() >= 25 && line[0] == '[' && line[24] == ']') {
        const auto timestamp = QDateTime::fromString(
            QString::fromUtf8(line.mid(1, 23)), "yyyy-MM-dd hh:mm:ss.zzz");
        if (timestamp.isValid()) {
          keep = timestamp >= cutoff;
        }
      }
      // Multiline kernel messages follow the timestamp of their first line.
      if (keep) {
        if (output.write(line) != line.size()) {
          success = false;
          break;
        }
        retainedBytes += line.size();
      } else {
        removed = true;
      }
    }
    const bool readOk = input.error() == QFileDevice::NoError;
    input.close();
    if (!readOk || output.error() != QFileDevice::NoError) {
      output.cancelWriting();
      success = false;
    } else if (!removed) {
      output.cancelWriting();
    } else if (retainedBytes == 0) {
      output.cancelWriting();
      success = QFile::remove(file.absoluteFilePath()) && success;
    } else {
      success = output.commit() && success;
    }
  }
  return success;
}
}  // namespace LogRetention
