#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include "core/CoreManagerProtocol.h"
#include "coremanager/CoreManagerServer.h"
#include "utils/AppPaths.h"
#include "utils/Logger.h"

namespace {
QString optionValue(const QStringList& args, const QString& name) {
  const int index = args.indexOf(name);
  if (index < 0 || index + 1 >= args.size()) {
    return QString();
  }
  return args.at(index + 1);
}

void applyRuntimeOptions(const QStringList& args) {
  if (args.contains("--portable")) {
    qputenv("SING_BOX_QT_PORTABLE", "1");
  }
  const QString dataDir = optionValue(args, "--data-dir");
  if (!dataDir.trimmed().isEmpty()) {
    qputenv("SING_BOX_QT_DATA_DIR", QDir(dataDir).absolutePath().toUtf8());
  }
}

QString defaultRuntimeConfigPath() {
  return QDir(appDataDir()).filePath("config.json");
}

int runCoreManager(QCoreApplication& app, bool serviceMode = false) {
  app.setApplicationName("sing-box-core-manager");
  const QStringList args = app.arguments();
  applyRuntimeOptions(args);
  Logger::instance().init();
  Logger::info("Core manager starting...");
  QString serverName = coreManagerServerName();
  for (int i = 1; i < args.size(); ++i) {
    if (args[i] == "--control-name" && i + 1 < args.size()) {
      serverName = args[i + 1];
      ++i;
    }
  }
  CoreManagerServer server;
  QString           error;
  if (!server.startListening(serverName, &error)) {
    Logger::error(error);
    return 1;
  }
  if (serviceMode) {
    Logger::info("Core manager service is running");
    server.setKeepKernelRunning(true, defaultRuntimeConfigPath());
  }
  return app.exec();
}

QString quoteWindowsArg(const QString& value) {
  QString escaped = value;
  escaped.replace('"', "\\\"");
  return QString("\"%1\"").arg(escaped);
}
}  // namespace

#ifdef Q_OS_WIN
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on

namespace {
SERVICE_STATUS        g_serviceStatus = {};
SERVICE_STATUS_HANDLE g_statusHandle  = nullptr;

QString moduleFilePath() {
  wchar_t     path[MAX_PATH] = {};
  const DWORD length         = GetModuleFileNameW(nullptr, path, MAX_PATH);
  return QString::fromWCharArray(path, static_cast<int>(length));
}

QString serviceNameFromModulePath() {
  const QString appDir =
      QDir::cleanPath(QFileInfo(moduleFilePath()).absolutePath());
  const QByteArray hash =
      QCryptographicHash::hash(appDir.toUtf8(), QCryptographicHash::Sha1);
  return QString("SingBoxQtCore-%1")
      .arg(QString::fromLatin1(hash.toHex().left(8)));
}

void setServiceStatus(DWORD state,
                      DWORD win32ExitCode = NO_ERROR,
                      DWORD waitHint      = 0) {
  if (!g_statusHandle) {
    return;
  }
  g_serviceStatus.dwServiceType             = SERVICE_WIN32_OWN_PROCESS;
  g_serviceStatus.dwCurrentState            = state;
  g_serviceStatus.dwWin32ExitCode           = win32ExitCode;
  g_serviceStatus.dwWaitHint                = waitHint;
  g_serviceStatus.dwControlsAccepted        = 0;
  g_serviceStatus.dwCheckPoint              = 0;
  g_serviceStatus.dwServiceSpecificExitCode = 0;
  if (state == SERVICE_RUNNING) {
    g_serviceStatus.dwControlsAccepted =
        SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
  } else if (state == SERVICE_START_PENDING || state == SERVICE_STOP_PENDING) {
    g_serviceStatus.dwCheckPoint = 1;
  }
  SetServiceStatus(g_statusHandle, &g_serviceStatus);
}

void WINAPI serviceControlHandler(DWORD control) {
  if (control != SERVICE_CONTROL_STOP && control != SERVICE_CONTROL_SHUTDOWN) {
    return;
  }
  setServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 3000);
  if (qApp) {
    QMetaObject::invokeMethod(
        qApp, &QCoreApplication::quit, Qt::QueuedConnection);
  }
}

QVector<QByteArray> commandLineArgs() {
  int                 argc = 0;
  LPWSTR*             argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  QVector<QByteArray> args;
  if (!argv) {
    args.append(moduleFilePath().toLocal8Bit());
    return args;
  }
  args.reserve(argc);
  for (int i = 0; i < argc; ++i) {
    args.append(QString::fromWCharArray(argv[i]).toLocal8Bit());
  }
  LocalFree(argv);
  return args;
}

void WINAPI serviceMain(DWORD, LPWSTR*) {
  const QString serviceName = serviceNameFromModulePath();
  g_statusHandle            = RegisterServiceCtrlHandlerW(
      reinterpret_cast<LPCWSTR>(serviceName.utf16()), serviceControlHandler);
  if (!g_statusHandle) {
    return;
  }
  setServiceStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

  QVector<QByteArray> encodedArgs = commandLineArgs();
  QVector<char*>      argv;
  argv.reserve(encodedArgs.size());
  for (QByteArray& arg : encodedArgs) {
    argv.append(arg.data());
  }
  int              argc = argv.size();
  QCoreApplication app(argc, argv.data());
  setServiceStatus(SERVICE_RUNNING);
  const int exitCode = runCoreManager(app, true);
  setServiceStatus(SERVICE_STOPPED,
                   exitCode == 0 ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR);
}

bool runServiceDispatcher() {
  const QString        serviceName = serviceNameFromModulePath();
  SERVICE_TABLE_ENTRYW table[]     = {
      {const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(serviceName.utf16())),
           serviceMain},
      {nullptr, nullptr}};
  return StartServiceCtrlDispatcherW(table);
}

QString serviceBinaryPath(const QStringList& args) {
  const QString exePath =
      QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
  QString dataDir = optionValue(args, "--data-dir");
  if (dataDir.trimmed().isEmpty()) {
    dataDir = QDir(QCoreApplication::applicationDirPath()).filePath("data");
  }
  QString controlName = optionValue(args, "--control-name");
  if (controlName.trimmed().isEmpty()) {
    controlName = coreManagerServerName();
  }
  return QString("%1 --service --control-name %2 --data-dir %3")
      .arg(quoteWindowsArg(exePath),
           quoteWindowsArg(controlName),
           quoteWindowsArg(QDir(dataDir).absolutePath()));
}

void setServiceDescription(SC_HANDLE service) {
  QString description =
      "Runs the Sing-Box Qt core manager before the UI starts.";
  SERVICE_DESCRIPTIONW desc = {};
  desc.lpDescription =
      const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(description.utf16()));
  ChangeServiceConfig2W(service, SERVICE_CONFIG_DESCRIPTION, &desc);
}

int installService(const QStringList& args) {
  const QString serviceName = coreManagerServiceName();
  const QString displayName = coreManagerServiceDisplayName();
  const QString binaryPath  = serviceBinaryPath(args);
  SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
  if (!scm) {
    return 1;
  }
  SC_HANDLE service = CreateServiceW(
      scm,
      reinterpret_cast<LPCWSTR>(serviceName.utf16()),
      reinterpret_cast<LPCWSTR>(displayName.utf16()),
      SERVICE_CHANGE_CONFIG | SERVICE_START | SERVICE_QUERY_STATUS,
      SERVICE_WIN32_OWN_PROCESS,
      SERVICE_AUTO_START,
      SERVICE_ERROR_NORMAL,
      reinterpret_cast<LPCWSTR>(binaryPath.utf16()),
      nullptr,
      nullptr,
      nullptr,
      nullptr,
      nullptr);
  if (!service && GetLastError() == ERROR_SERVICE_EXISTS) {
    service = OpenServiceW(
        scm,
        reinterpret_cast<LPCWSTR>(serviceName.utf16()),
        SERVICE_CHANGE_CONFIG | SERVICE_START | SERVICE_QUERY_STATUS);
    if (service) {
      ChangeServiceConfigW(service,
                           SERVICE_WIN32_OWN_PROCESS,
                           SERVICE_AUTO_START,
                           SERVICE_ERROR_NORMAL,
                           reinterpret_cast<LPCWSTR>(binaryPath.utf16()),
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr,
                           reinterpret_cast<LPCWSTR>(displayName.utf16()));
    }
  }
  if (!service) {
    CloseServiceHandle(scm);
    return 1;
  }
  setServiceDescription(service);
  const bool started = StartServiceW(service, 0, nullptr) ||
                       GetLastError() == ERROR_SERVICE_ALREADY_RUNNING;
  CloseServiceHandle(service);
  CloseServiceHandle(scm);
  return started ? 0 : 1;
}

DWORD queryServiceState(SC_HANDLE service) {
  SERVICE_STATUS_PROCESS status = {};
  DWORD                  needed = 0;
  if (!QueryServiceStatusEx(service,
                            SC_STATUS_PROCESS_INFO,
                            reinterpret_cast<LPBYTE>(&status),
                            sizeof(status),
                            &needed)) {
    return 0;
  }
  return status.dwCurrentState;
}

bool waitForServiceState(SC_HANDLE service,
                         DWORD     expectedState,
                         int       timeoutMs) {
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < timeoutMs) {
    const DWORD state = queryServiceState(service);
    if (state == expectedState) {
      return true;
    }
    Sleep(200);
  }
  return queryServiceState(service) == expectedState;
}

int stopService(bool deleteAfterStop) {
  const QString serviceName = coreManagerServiceName();
  SC_HANDLE     scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
  if (!scm) {
    return 1;
  }
  const DWORD serviceAccess =
      SERVICE_STOP | SERVICE_QUERY_STATUS | (deleteAfterStop ? DELETE : 0);
  SC_HANDLE service = OpenServiceW(
      scm, reinterpret_cast<LPCWSTR>(serviceName.utf16()), serviceAccess);
  if (!service) {
    const DWORD error = GetLastError();
    CloseServiceHandle(scm);
    return deleteAfterStop && error == ERROR_SERVICE_DOES_NOT_EXIST ? 0 : 1;
  }
  bool stopped = queryServiceState(service) == SERVICE_STOPPED;
  if (!stopped) {
    SERVICE_STATUS status = {};
    if (!ControlService(service, SERVICE_CONTROL_STOP, &status) &&
        GetLastError() != ERROR_SERVICE_NOT_ACTIVE) {
      CloseServiceHandle(service);
      CloseServiceHandle(scm);
      return 1;
    }
    stopped = waitForServiceState(service, SERVICE_STOPPED, 10000);
  }
  bool deleted = true;
  if (deleteAfterStop) {
    deleted = DeleteService(service) ||
              GetLastError() == ERROR_SERVICE_MARKED_FOR_DELETE;
  }
  CloseServiceHandle(service);
  CloseServiceHandle(scm);
  return stopped && deleted ? 0 : 1;
}

int startService() {
  const QString serviceName = coreManagerServiceName();
  SC_HANDLE     scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
  if (!scm) {
    return 1;
  }
  SC_HANDLE service =
      OpenServiceW(scm,
                   reinterpret_cast<LPCWSTR>(serviceName.utf16()),
                   SERVICE_START | SERVICE_QUERY_STATUS);
  if (!service) {
    CloseServiceHandle(scm);
    return 1;
  }
  const bool started = StartServiceW(service, 0, nullptr) ||
                       GetLastError() == ERROR_SERVICE_ALREADY_RUNNING;
  CloseServiceHandle(service);
  CloseServiceHandle(scm);
  return started ? 0 : 1;
}
}  // namespace
#endif

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
  for (int i = 1; i < argc; ++i) {
    if (QString::fromLocal8Bit(argv[i]) == "--service") {
      return runServiceDispatcher() ? 0 : 1;
    }
  }
#endif

  QCoreApplication app(argc, argv);
#ifdef Q_OS_WIN
  const QStringList args = app.arguments();
  if (args.contains("--install-service")) {
    return installService(args);
  }
  if (args.contains("--uninstall-service")) {
    return stopService(true);
  }
  if (args.contains("--start-service")) {
    return startService();
  }
  if (args.contains("--stop-service")) {
    return stopService(false);
  }
#endif
  return runCoreManager(app);
}
