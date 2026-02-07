#ifndef COREMANAGERPROTOCOL_H
#define COREMANAGERPROTOCOL_H
#include <QtGlobal>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QString>

inline QString coreManagerServerName() {
  QString user = qEnvironmentVariable("USERNAME");
  if (user.isEmpty()) {
    user = qEnvironmentVariable("USER");
  }
  const QString baseName =
      !user.isEmpty() ? QString("sing-box-qt-core-%1").arg(user.toLower())
                      : QString("sing-box-qt-core");
  const QString appDir = QDir::cleanPath(
      QFileInfo(QCoreApplication::applicationFilePath()).absolutePath());
  const QByteArray hash = QCryptographicHash::hash(
      appDir.toUtf8(), QCryptographicHash::Sha1);
  const QString suffix = QString::fromLatin1(hash.toHex().left(8));
  return QString("%1-%2").arg(baseName, suffix);
}

inline QString coreManagerExecutableName() {
#ifdef Q_OS_WIN
  return "sing-box-core-manager.exe";
#else
  return "sing-box-core-manager";
#endif
}
#endif  // COREMANAGERPROTOCOL_H
