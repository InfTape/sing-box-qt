#include "storage/ConfigIO.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include "utils/AppPaths.h"
#include "utils/Logger.h"
namespace ConfigIO {
QString getConfigDir() {
  const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  const QString dataDir = appDataDir();
  QDir          dir(dataDir);
  if (!dir.exists()) {
    dir.mkpath(".");
  }
  const QString newConfig  = QDir(dataDir).filePath("config.json");
  const QString oldConfig1 = QDir(baseDir).filePath("sing-box-qt/config.json");
  const QString oldConfig2 = QDir(baseDir).filePath("config.json");
  if (!QFile::exists(newConfig)) {
    if (QFile::exists(oldConfig1)) {
      QFile::copy(oldConfig1, newConfig);
    } else if (QFile::exists(oldConfig2)) {
      QFile::copy(oldConfig2, newConfig);
    }
  }
  return dataDir;
}
QString getActiveConfigPath() {
  return getConfigDir() + "/config.json";
}
QJsonObject loadConfig(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    Logger::warn(QString("Failed to open config file: %1").arg(path));
    return QJsonObject();
  }
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();
  return doc.object();
}
bool saveConfig(const QString& path, const QJsonObject& config) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    Logger::error(QString("Failed to write config file: %1").arg(path));
    return false;
  }
  QJsonDocument doc(config);
  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();
  Logger::info(QString("Config saved: %1").arg(path));
  return true;
}
}  // namespace ConfigIO
