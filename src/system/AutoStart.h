#ifndef AUTOSTART_H
#define AUTOSTART_H
#include <QString>

class AutoStart {
 public:
  static bool isSupported();
  static bool isEnabled(const QString& appName = QString());
  static bool setEnabled(bool enabled, const QString& appName = QString());
};
#endif  // AUTOSTART_H
