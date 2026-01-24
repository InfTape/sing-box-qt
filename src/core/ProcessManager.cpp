#include "ProcessManager.h"
#include "utils/Logger.h"
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#else
#include <errno.h>
#include <signal.h>
#include <string.h>
#endif

ProcessManager::ProcessManager(QObject *parent)
    : QObject(parent)
{
}

QList<ProcessInfo> ProcessManager::findProcessesByName(const QString &name)
{
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
                info.pid = pe32.th32ProcessID;
                info.name = processName;
                processes.append(info);
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
#else
    const QString psPath = QStandardPaths::findExecutable("ps");
    if (psPath.isEmpty()) {
        Logger::warn("ps not found; cannot enumerate processes");
        return processes;
    }

    QProcess proc;
    proc.start(psPath, {"-axo", "pid=,comm="});
    if (!proc.waitForFinished(2000)) {
        proc.kill();
        return processes;
    }

    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;
        const QStringList parts = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 2) continue;
        bool ok = false;
        const qint64 pid = parts.at(0).toLongLong(&ok);
        if (!ok) continue;
        const QString processName = parts.at(1);
        if (processName.compare(name, Qt::CaseInsensitive) == 0) {
            ProcessInfo info;
            info.pid = pid;
            info.name = processName;
            processes.append(info);
        }
    }
#endif

    return processes;
}

bool ProcessManager::isProcessRunning(const QString &name)
{
    return !findProcessesByName(name).isEmpty();
}

bool ProcessManager::isProcessRunning(qint64 pid)
{
#ifdef Q_OS_WIN
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (hProcess) {
        DWORD exitCode;
        if (GetExitCodeProcess(hProcess, &exitCode)) {
            CloseHandle(hProcess);
            return exitCode == STILL_ACTIVE;
        }
        CloseHandle(hProcess);
    }
    return false;
#else
    if (pid <= 0) return false;
    if (::kill(static_cast<pid_t>(pid), 0) == 0) {
        return true;
    }
    return errno == EPERM;
#endif
}

bool ProcessManager::killProcess(qint64 pid)
{
#ifdef Q_OS_WIN
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
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
#else
    if (pid <= 0) return false;
    if (::kill(static_cast<pid_t>(pid), SIGTERM) == 0) {
        Logger::info(QString("Process terminated: PID=%1").arg(pid));
        return true;
    }
    Logger::warn(QString("Failed to terminate process: PID=%1, error=%2")
        .arg(pid)
        .arg(QString::fromLocal8Bit(strerror(errno))));
    return false;
#endif
}

bool ProcessManager::killProcessByName(const QString &name)
{
    QList<ProcessInfo> processes = findProcessesByName(name);
    bool allKilled = true;

    for (const ProcessInfo &proc : processes) {
        if (!killProcess(proc.pid)) {
            allKilled = false;
        }
    }

    return allKilled;
}

void ProcessManager::cleanupKernelProcesses()
{
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

    for (const ProcessInfo &proc : processes) {
        Logger::info(QString("Leftover process found: %1 (PID: %2)").arg(proc.name).arg(proc.pid));
        killProcess(proc.pid);
    }
}
