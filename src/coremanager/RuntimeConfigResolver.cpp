#include "coremanager/RuntimeConfigResolver.h"

#include <QDir>
#include <QFileInfo>

#include "storage/DatabaseService.h"
#include "utils/AppPaths.h"
#include "utils/Logger.h"

namespace {
QString normalizedPath(const QString& path) {
  const QString trimmed = path.trimmed();
  if (trimmed.isEmpty()) {
    return QString();
  }
  QFileInfo info(QDir::fromNativeSeparators(trimmed));
  if (info.isRelative()) {
    info.setFile(QDir(appDataDir()).filePath(trimmed));
  }
  return QDir::cleanPath(info.absoluteFilePath());
}

bool isConfigFile(const QString& path) {
  const QFileInfo info(path);
  return info.exists() && info.isFile();
}
}  // namespace

namespace RuntimeConfigResolver {
QString defaultConfigPath() {
  return QDir(appDataDir()).filePath("config.json");
}

QString selectConfigPath(const QString& persistedConfigPath,
                         const QString& fallbackConfigPath) {
  const QString persisted = normalizedPath(persistedConfigPath);
  if (!persisted.isEmpty() && isConfigFile(persisted)) {
    return persisted;
  }
  return normalizedPath(fallbackConfigPath);
}

QString resolveConfigPath() {
  QString persistedConfigPath;
  if (DatabaseService::instance().init()) {
    persistedConfigPath = DatabaseService::instance().getActiveConfigPath();
  } else {
    Logger::warn(
        "Failed to read the active config path; using the default config");
  }

  const QString fallback  = defaultConfigPath();
  const QString selected  = selectConfigPath(persistedConfigPath, fallback);
  const QString persisted = normalizedPath(persistedConfigPath);
  if (!persisted.isEmpty() && selected == persisted) {
    Logger::info(
        QString("Restoring active runtime config: %1").arg(selected));
  } else {
    if (!persisted.isEmpty()) {
      Logger::warn(QString("Persisted runtime config not found: %1")
                       .arg(persisted));
    }
    Logger::info(QString("Using default runtime config: %1").arg(selected));
  }
  return selected;
}
}  // namespace RuntimeConfigResolver
