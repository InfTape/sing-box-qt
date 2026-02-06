#ifndef APPPATHS_H
#define APPPATHS_H
#include <QDir>
#include <QStandardPaths>
#include <QString>
inline QString appDataRoot() {
  QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#ifdef Q_OS_WIN
  QDir dir(path);
  while (dir.exists() && dir.dirName().compare("Roaming", Qt::CaseInsensitive) != 0) {
    if (!dir.cdUp()) {
      break;
    }
  }
  if (dir.dirName().compare("Roaming", Qt::CaseInsensitive) == 0) {
    return dir.absolutePath();
  }
#endif
  return QDir(path).absolutePath();
}
inline QString appDataDir() {
  return QDir(appDataRoot()).filePath("sing-box-qt");
}
#endif  // APPPATHS_H
