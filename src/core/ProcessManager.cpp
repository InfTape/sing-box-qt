#include "ProcessManager.h"
#include "utils/Logger.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
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
    // TODO: implement for Unix.
    return false;
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
    // TODO: implement for Unix.
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
