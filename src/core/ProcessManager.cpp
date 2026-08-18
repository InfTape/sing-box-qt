#include "ProcessManager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include "utils/Logger.h"
#ifdef Q_OS_WIN
#include <windows.h>  // must be first for Windows types/macros
#include <psapi.h>
#include <tlhelp32.h>
#elif defined(Q_OS_LINUX)
#include <cerrno>
#include <csignal>
#include <unistd.h>
#endif

namespace {
QString normalizeProcessPath(const QString& path) {
  if (path.trimmed().isEmpty()) {
    return QString();
  }
  QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef Q_OS_WIN
  normalized = normalized.toLower();
#endif
  return normalized;
}

#ifdef Q_OS_WIN
QString queryProcessPathByPid(DWORD pid) {
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!process) {
    return QString();
  }
  wchar_t buffer[32768];
  DWORD   length = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
  QString result;
  if (QueryFullProcessImageNameW(process, 0, buffer, &length)) {
    result = QString::fromWCharArray(buffer, static_cast<int>(length));
  }
  CloseHandle(process);
  return result;
}
#elif defined(Q_OS_LINUX)
ProcessInfo queryProcessInfo(qint64 pid) {
  ProcessInfo info{pid, QString(), QString()};
  QFile       comm(QString("/proc/%1/comm").arg(pid));
  if (comm.open(QIODevice::ReadOnly | QIODevice::Text)) {
    info.name = QString::fromUtf8(comm.readAll()).trimmed();
  }
  info.path = QFileInfo(QString("/proc/%1/exe").arg(pid)).symLinkTarget();
  if (info.name.isEmpty() && !info.path.isEmpty()) {
    info.name = QFileInfo(info.path).fileName();
  }
  return info;
}
#endif
}  // namespace

ProcessManager::ProcessManager(QObject* parent) : QObject(parent) {}

QList<ProcessInfo> ProcessManager::findProcessesByName(const QString& name) {
  QList<ProcessInfo> processes;
#ifdef Q_OS_WIN
  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE) {
    return processes;
  }
  PROCESSENTRY32W pe32;
  pe32.dwSize = sizeof(pe32);
  if (Process32FirstW(hSnapshot, &pe32)) {
    do {
      QString processName = QString::fromWCharArray(pe32.szExeFile);
      if (processName.compare(name, Qt::CaseInsensitive) == 0) {
        ProcessInfo info;
        info.pid  = pe32.th32ProcessID;
        info.name = processName;
        info.path = queryProcessPathByPid(static_cast<DWORD>(info.pid));
        processes.append(info);
      }
    } while (Process32NextW(hSnapshot, &pe32));
  }
  CloseHandle(hSnapshot);
#elif defined(Q_OS_LINUX)
  QDir proc("/proc");
  const QFileInfoList entries =
      proc.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  for (const QFileInfo& entry : entries) {
    bool         ok  = false;
    const qint64 pid = entry.fileName().toLongLong(&ok);
    if (!ok) {
      continue;
    }
    const ProcessInfo info = queryProcessInfo(pid);
    if (info.name.compare(name, Qt::CaseSensitive) == 0) {
      processes.append(info);
    }
  }
#endif
  return processes;
}

QList<ProcessInfo> ProcessManager::findProcessesByPath(const QString& path) {
  QList<ProcessInfo> processes;
#ifdef Q_OS_WIN
  const QString targetPath =
      normalizeProcessPath(QFileInfo(path).absoluteFilePath());
  if (targetPath.isEmpty()) {
    return processes;
  }
  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE) {
    return processes;
  }
  PROCESSENTRY32W pe32;
  pe32.dwSize = sizeof(pe32);
  if (Process32FirstW(hSnapshot, &pe32)) {
    do {
      const DWORD   pid       = pe32.th32ProcessID;
      const QString imagePath = queryProcessPathByPid(pid);
      if (normalizeProcessPath(imagePath) != targetPath) {
        continue;
      }
      ProcessInfo info;
      info.pid  = pid;
      info.name = QString::fromWCharArray(pe32.szExeFile);
      info.path = imagePath;
      processes.append(info);
    } while (Process32NextW(hSnapshot, &pe32));
  }
  CloseHandle(hSnapshot);
#elif defined(Q_OS_LINUX)
  const QString targetPath =
      normalizeProcessPath(QFileInfo(path).absoluteFilePath());
  if (targetPath.isEmpty()) {
    return processes;
  }
  QDir proc("/proc");
  const QFileInfoList entries =
      proc.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  for (const QFileInfo& entry : entries) {
    bool         ok  = false;
    const qint64 pid = entry.fileName().toLongLong(&ok);
    if (!ok) {
      continue;
    }
    const ProcessInfo info = queryProcessInfo(pid);
    if (normalizeProcessPath(info.path) == targetPath) {
      processes.append(info);
    }
  }
#else
  Q_UNUSED(path)
#endif
  return processes;
}

bool ProcessManager::isProcessRunning(const QString& name) {
  return !findProcessesByName(name).isEmpty();
}

bool ProcessManager::isProcessRunning(qint64 pid) {
#ifdef Q_OS_WIN
  HANDLE hProcess = OpenProcess(
      PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
  if (hProcess) {
    DWORD exitCode;
    if (GetExitCodeProcess(hProcess, &exitCode)) {
      CloseHandle(hProcess);
      return exitCode == STILL_ACTIVE;
    }
    CloseHandle(hProcess);
  }
  return false;
#elif defined(Q_OS_LINUX)
  if (pid <= 0) {
    return false;
  }
  if (::kill(static_cast<pid_t>(pid), 0) == 0) {
    return true;
  }
  return errno == EPERM;
#else
  Q_UNUSED(pid)
  return false;
#endif
}

bool ProcessManager::killProcess(qint64 pid) {
#ifdef Q_OS_WIN
  HANDLE hProcess =
      OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
  if (hProcess) {
    BOOL result = TerminateProcess(hProcess, 0);
    CloseHandle(hProcess);
    if (result) {
      Logger::info(QString("Process terminated: PID=%1").arg(pid));
      return true;
    }
  }
  Logger::warn(QString("Failed to terminate process: PID=%1").arg(pid));
  return false;
#elif defined(Q_OS_LINUX)
  if (pid <= 0 || pid == static_cast<qint64>(::getpid())) {
    return false;
  }
  if (::kill(static_cast<pid_t>(pid), SIGTERM) == 0) {
    Logger::info(QString("Process termination requested: PID=%1").arg(pid));
    return true;
  }
  Logger::warn(QString("Failed to terminate process: PID=%1, errno=%2")
                   .arg(pid)
                   .arg(errno));
  return false;
#else
  Q_UNUSED(pid)
  return false;
#endif
}

bool ProcessManager::killProcessByName(const QString& name) {
  QList<ProcessInfo> processes = findProcessesByName(name);
  bool               allKilled = true;
  for (const ProcessInfo& proc : processes) {
    if (!killProcess(proc.pid)) {
      allKilled = false;
    }
  }
  return allKilled;
}

bool ProcessManager::killProcessByPath(const QString& path) {
  QList<ProcessInfo> processes = findProcessesByPath(path);
  bool               allKilled = true;
  for (const ProcessInfo& proc : processes) {
    if (!killProcess(proc.pid)) {
      allKilled = false;
    }
  }
  return allKilled;
}

void ProcessManager::cleanupKernelProcesses() {
  Logger::info("Cleaning up leftover kernel processes...");
#ifdef Q_OS_WIN
  QString kernelName = "sing-box.exe";
#else
  QString kernelName = "sing-box";
#endif
  QList<ProcessInfo> processes = findProcessesByName(kernelName);
  if (processes.isEmpty()) {
    Logger::info("No leftover kernel processes found");
    return;
  }
  for (const ProcessInfo& proc : processes) {
    Logger::info(QString("Leftover process found: %1 (PID: %2)")
                     .arg(proc.name)
                     .arg(proc.pid));
    killProcess(proc.pid);
  }
}
