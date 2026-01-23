#ifndef PROCESSMANAGER_H
#define PROCESSMANAGER_H

#include <QObject>
#include <QString>
#include <QList>

struct ProcessInfo {
    qint64 pid;
    QString name;
    QString path;
};

class ProcessManager : public QObject
{
    Q_OBJECT

public:
    explicit ProcessManager(QObject *parent = nullptr);

    // Find processes.
    static QList<ProcessInfo> findProcessesByName(const QString &name);
    static bool isProcessRunning(const QString &name);
    static bool isProcessRunning(qint64 pid);

    // Terminate processes.
    static bool killProcess(qint64 pid);
    static bool killProcessByName(const QString &name);

    // Cleanup leftover kernel processes.
    static void cleanupKernelProcesses();
};

#endif // PROCESSMANAGER_H
