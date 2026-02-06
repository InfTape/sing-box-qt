#include "utils/subscription/SubscriptionFormat.h"
#include <QDateTime>
#include <QObject>

namespace SubscriptionFormat {
QString formatBytes(qint64 bytes) {
  if (bytes <= 0) {
    return "0 B";
  }
  static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
  double             value   = static_cast<double>(bytes);
  int                index   = 0;
  while (value >= 1024.0 && index < 4) {
    value /= 1024.0;
    index++;
  }
  return QString::number(value, 'f', 2) + " " + units[index];
}

QString formatTimestamp(qint64 ms) {
  if (ms <= 0) {
    return QObject::tr("Never updated");
  }
  return QDateTime::fromMSecsSinceEpoch(ms).toString("yyyy-MM-dd HH:mm:ss");
}

QString formatExpireTime(qint64 seconds) {
  if (seconds <= 0) {
    return QString();
  }
  return QObject::tr("Expires: %1")
      .arg(QDateTime::fromSecsSinceEpoch(seconds).toString("yyyy-MM-dd HH:mm"));
}
}  // namespace SubscriptionFormat
