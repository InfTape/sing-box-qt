#include "utils/home/HomeFormat.h"
namespace HomeFormat {
QString bytes(qint64 bytes) {
  if (bytes <= 0) return QStringLiteral("0 B");
  static const char* units[]   = {"B", "KB", "MB", "GB", "TB"};
  int                unitIndex = 0;
  double             size      = bytes;

  while (size >= 1024 && unitIndex < 4) {
    size /= 1024;
    unitIndex++;
  }

  return QString::number(size, 'f', unitIndex > 0 ? 2 : 0) + " " +
         units[unitIndex];
}
QString duration(int seconds) {
  int hours   = seconds / 3600;
  int minutes = (seconds % 3600) / 60;
  int secs    = seconds % 60;

  if (hours > 0) {
    return QString("%1:%2:%3")
        .arg(hours)
        .arg(minutes, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
  }
  return QString("%1:%2")
      .arg(minutes, 2, 10, QChar('0'))
      .arg(secs, 2, 10, QChar('0'));
}
}  // namespace HomeFormat
