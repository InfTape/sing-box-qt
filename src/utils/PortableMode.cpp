#include "utils/PortableMode.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace {
bool parseTruthy(const QString& value) {
  const QString normalized = value.trimmed().toLower();
  return normalized == "1" || normalized == "true" || normalized == "yes" ||
         normalized == "on";
}

bool hasPortableMarker(const QString& rootDir) {
  const QDir dir(rootDir);
  return QFileInfo::exists(dir.filePath("portable.flag")) ||
         QFileInfo::exists(dir.filePath("portable")) ||
         QFileInfo::exists(dir.filePath(".portable"));
}
}  // namespace

namespace PortableMode {
QString appRootDir() {
  const QFileInfo info(QCoreApplication::applicationFilePath());
  return info.absolutePath();
}

QString portableDataDir() {
  return QDir(appRootDir()).filePath("data");
}

bool isPortableEnabled() {
  static int cached = -1;
  if (cached >= 0) {
    return cached == 1;
  }

  bool requested = false;
  const QStringList args = QCoreApplication::arguments();
  requested = args.contains("--portable");

  if (!requested) {
    requested = parseTruthy(qEnvironmentVariable("SING_BOX_QT_PORTABLE"));
  }
  if (!requested) {
    requested = hasPortableMarker(appRootDir());
  }

  if (!requested) {
    cached = 0;
    return false;
  }

  const QString dataDir = portableDataDir();
  if (!QDir().mkpath(dataDir)) {
    cached = 0;
    return false;
  }

  cached = 1;
  return true;
}
}  // namespace PortableMode
