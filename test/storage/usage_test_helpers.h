#pragma once
#include <QTemporaryDir>
#include "storage/DatabaseService.h"

// Never clear or rewrite statistics in the caller's data directory.
struct UsageDatabaseScope {
  QTemporaryDir directory;
  QByteArray previous = qgetenv("SING_BOX_QT_DATA_DIR");
  bool wasSet = qEnvironmentVariableIsSet("SING_BOX_QT_DATA_DIR");
  UsageDatabaseScope() {
    DatabaseService::instance().close();
    qputenv("SING_BOX_QT_DATA_DIR", directory.path().toUtf8());
    DatabaseService::instance().init();
  }
  ~UsageDatabaseScope() {
    DatabaseService::instance().close();
    if (wasSet) qputenv("SING_BOX_QT_DATA_DIR", previous);
    else qunsetenv("SING_BOX_QT_DATA_DIR");
    DatabaseService::instance().init();
  }
};
