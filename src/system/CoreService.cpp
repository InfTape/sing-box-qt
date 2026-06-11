#include "CoreService.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include "core/CoreManagerProtocol.h"
#include "system/AdminHelper.h"
#include "utils/AppPaths.h"

#ifdef Q_OS_WIN
// clang-format off
#include <windows.h>
// clang-format on
#endif

namespace {
QString coreManagerPath() {
  return QDir(QCoreApplication::applicationDirPath())
      .filePath(coreManagerExecutableName());
}

QStringList serviceCommandArgs(bool enabled) {
  QStringList args;
  args << (enabled ? "--install-service" : "--uninstall-service");
  if (enabled) {
    args << "--control-name" << coreManagerServerName();
    args << "--data-dir" << appDataDir();
  }
  return args;
}

bool runCoreManagerCommand(bool enabled) {
  const QString path = coreManagerPath();
  if (path.isEmpty() || !QFile::exists(path)) {
    return false;
  }
  const QStringList args = serviceCommandArgs(enabled);
  if (!AdminHelper::isAdmin()) {
    return AdminHelper::runAsAdminAndWait(path, args, 30000);
  }
  QProcess process;
  process.start(path, args);
  if (!process.waitForFinished(30000)) {
    process.kill();
    return false;
  }
  return process.exitStatus() == QProcess::NormalExit &&
         process.exitCode() == 0;
}
}  // namespace

bool CoreService::isSupported() {
#ifdef Q_OS_WIN
  return true;
#else
  return false;
#endif
}

bool CoreService::isInstalled() {
#ifdef Q_OS_WIN
  const QString serviceName = coreManagerServiceName();
  SC_HANDLE     scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
  if (!scm) {
    return false;
  }
  SC_HANDLE service =
      OpenServiceW(scm,
                   reinterpret_cast<LPCWSTR>(serviceName.utf16()),
                   SERVICE_QUERY_STATUS);
  if (!service) {
    CloseServiceHandle(scm);
    return false;
  }
  CloseServiceHandle(service);
  CloseServiceHandle(scm);
  return true;
#else
  return false;
#endif
}

bool CoreService::setEnabled(bool enabled) {
  if (!isSupported()) {
    return false;
  }
  if (isInstalled() == enabled) {
    return true;
  }
  return runCoreManagerCommand(enabled);
}
