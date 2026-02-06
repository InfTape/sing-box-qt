#include "storage/SubscriptionConfigStore.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include "app/interfaces/ConfigRepository.h"

namespace {
QString sanitizeFileName(const QString& name) {
  QString safe = name.toLower();
  safe.replace(QRegularExpression("[^a-z0-9-_]+"), "-");
  safe.replace(QRegularExpression("-+"), "-");
  safe.remove(QRegularExpression("^-|-$"));
  if (safe.isEmpty()) {
    safe = "subscription";
  }
  return safe;
}
}  // namespace

namespace SubscriptionConfigStore {
QString generateConfigFileName(const QString& name) {
  const QString safe = sanitizeFileName(name);
  return QString("%1-%2.json")
      .arg(safe)
      .arg(QDateTime::currentMSecsSinceEpoch());
}

bool saveConfigWithNodes(ConfigRepository* cfg,
                         const QJsonArray& nodes,
                         const QString&    targetPath) {
  return cfg ? cfg->generateConfigWithNodes(nodes, targetPath) : false;
}

bool saveOriginalConfig(ConfigRepository* cfg,
                        const QString&    content,
                        const QString&    targetPath) {
  QJsonParseError err;
  QJsonDocument   doc = QJsonDocument::fromJson(content.toUtf8(), &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    return false;
  }
  if (!cfg) {
    return false;
  }
  QJsonObject config = doc.object();
  cfg->applyPortSettings(config);
  return cfg->saveConfig(targetPath, config);
}

bool rollbackSubscriptionConfig(const QString& configPath) {
  QFileInfo pathInfo(configPath);
  if (!pathInfo.exists()) {
    return false;
  }
  const QString backupPath = configPath + ".bak";
  if (!QFile::exists(backupPath)) {
    return false;
  }
  QFile::remove(configPath);
  return QFile::copy(backupPath, configPath);
}

bool deleteSubscriptionConfig(const QString& configPath) {
  if (configPath.isEmpty()) {
    return false;
  }
  if (QFile::exists(configPath)) {
    QFile::remove(configPath);
  }
  const QString backup = configPath + ".bak";
  if (QFile::exists(backup)) {
    QFile::remove(backup);
  }
  return true;
}
}  // namespace SubscriptionConfigStore
