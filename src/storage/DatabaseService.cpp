#include "DatabaseService.h"
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include "utils/AppPaths.h"
#include "utils/Logger.h"

DatabaseService& DatabaseService::instance() {
  static DatabaseService instance;
  return instance;
}

DatabaseService::DatabaseService(QObject* parent)
    : QObject(parent), m_initialized(false) {}

DatabaseService::~DatabaseService() {
  close();
}

bool DatabaseService::init() {
  if (m_initialized) {
    return true;
  }
  QString dbPath = getDatabasePath();
  // Ensure the directory exists.
  QDir dir(QFileInfo(dbPath).absolutePath());
  if (!dir.exists()) {
    dir.mkpath(".");
  }
  m_db = QSqlDatabase::addDatabase("QSQLITE");
  m_db.setDatabaseName(dbPath);
  if (!m_db.open()) {
    Logger::error(
        QString("Failed to open database: %1").arg(m_db.lastError().text()));
    return false;
  }
  if (!createTables()) {
    return false;
  }
  m_initialized = true;
  Logger::info(QString("Database initialized: %1").arg(dbPath));
  return true;
}

void DatabaseService::close() {
  if (m_db.isOpen()) {
    m_db.close();
  }
  m_initialized = false;
}

bool DatabaseService::createTables() {
  QSqlQuery query(m_db);
  // Key-value store table.
  QString sql = R"(
        CREATE TABLE IF NOT EXISTS kv_store (
            key TEXT PRIMARY KEY,
            value TEXT,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
  if (!query.exec(sql)) {
    Logger::error(
        QString("Failed to create table: %1").arg(query.lastError().text()));
    return false;
  }
  return true;
}

QString DatabaseService::getDatabasePath() const {
  const QString newDir  = appDataDir();
  const QString newPath = QDir(newDir).filePath("sing-box.db");
  if (!QFile::exists(newPath)) {
    const QString oldBase =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString oldPath1 = QDir(oldBase).filePath("sing-box-qt/sing-box.db");
    const QString oldPath2 = QDir(oldBase).filePath("sing-box.db");
    if (QFile::exists(oldPath1)) {
      QFile::copy(oldPath1, newPath);
    } else if (QFile::exists(oldPath2)) {
      QFile::copy(oldPath2, newPath);
    }
  }
  return newPath;
}

QString DatabaseService::getValue(const QString& key,
                                  const QString& defaultValue) {
  QSqlQuery query(m_db);
  query.prepare("SELECT value FROM kv_store WHERE key = ?");
  query.addBindValue(key);
  if (query.exec() && query.next()) {
    return query.value(0).toString();
  }
  return defaultValue;
}

bool DatabaseService::setValue(const QString& key, const QString& value) {
  QSqlQuery query(m_db);
  query.prepare(R"(
        INSERT OR REPLACE INTO kv_store (key, value, updated_at)
        VALUES (?, ?, CURRENT_TIMESTAMP)
    )");
  query.addBindValue(key);
  query.addBindValue(value);
  if (!query.exec()) {
    Logger::error(
        QString("Failed to save data: %1").arg(query.lastError().text()));
    return false;
  }
  return true;
}

QJsonObject DatabaseService::getAppConfig() {
  QString json = getValue("app_config", "{}");
  return QJsonDocument::fromJson(json.toUtf8()).object();
}

bool DatabaseService::saveAppConfig(const QJsonObject& config) {
  QString json =
      QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Compact));
  return setValue("app_config", json);
}

QJsonObject DatabaseService::getThemeConfig() {
  QString json =
      getValue("theme_config", R"({"theme":"dark","primaryColor":"#e94560"})");
  return QJsonDocument::fromJson(json.toUtf8()).object();
}

bool DatabaseService::saveThemeConfig(const QJsonObject& config) {
  QString json =
      QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Compact));
  return setValue("theme_config", json);
}

QString DatabaseService::getLocale() {
  return getValue("locale", "zh_CN");
}

bool DatabaseService::saveLocale(const QString& locale) {
  return setValue("locale", locale);
}

QJsonArray DatabaseService::getSubscriptions() {
  QString json = getValue("subscriptions", "[]");
  return QJsonDocument::fromJson(json.toUtf8()).array();
}

bool DatabaseService::saveSubscriptions(const QJsonArray& subscriptions) {
  QString json = QString::fromUtf8(
      QJsonDocument(subscriptions).toJson(QJsonDocument::Compact));
  return setValue("subscriptions", json);
}

int DatabaseService::getActiveSubscriptionIndex() {
  return getValue("active_subscription_index", "-1").toInt();
}

bool DatabaseService::saveActiveSubscriptionIndex(int index) {
  return setValue("active_subscription_index", QString::number(index));
}

QString DatabaseService::getActiveConfigPath() {
  return getValue("active_config_path", "");
}

bool DatabaseService::saveActiveConfigPath(const QString& path) {
  return setValue("active_config_path", path);
}

QJsonArray DatabaseService::getSubscriptionNodes(const QString& id) {
  QString key  = QString("sub_nodes_%1").arg(id);
  QString json = getValue(key, "[]");
  return QJsonDocument::fromJson(json.toUtf8()).array();
}

bool DatabaseService::saveSubscriptionNodes(const QString&    id,
                                            const QJsonArray& nodes) {
  QString key = QString("sub_nodes_%1").arg(id);
  QString json =
      QString::fromUtf8(QJsonDocument(nodes).toJson(QJsonDocument::Compact));
  return setValue(key, json);
}

QJsonObject DatabaseService::getDataUsage() {
  QString json = getValue("data_usage_v1", "{}");
  return QJsonDocument::fromJson(json.toUtf8()).object();
}

bool DatabaseService::saveDataUsage(const QJsonObject& payload) {
  QString json =
      QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));
  return setValue("data_usage_v1", json);
}

bool DatabaseService::clearDataUsage() {
  return setValue("data_usage_v1", "{}");
}
