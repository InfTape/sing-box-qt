#ifndef COREMANAGERPROTOCOL_H
#define COREMANAGERPROTOCOL_H
#include <QtGlobal>
#include <QString>
inline QString coreManagerServerName() {
  QString user = qEnvironmentVariable("USERNAME");
  if (user.isEmpty()) {
    user = qEnvironmentVariable("USER");
  }
  if (!user.isEmpty()) {
    return QString("sing-box-qt-core-%1").arg(user.toLower());
  }
  return QString("sing-box-qt-core");
}
inline QString coreManagerExecutableName() {
#ifdef Q_OS_WIN
  return "sing-box-core-manager.exe";
#else
  return "sing-box-core-manager";
#endif
}
#endif  // COREMANAGERPROTOCOL_H
