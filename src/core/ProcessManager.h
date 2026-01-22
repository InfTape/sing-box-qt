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
    
    // 查找进程
    static QList<ProcessInfo> findProcessesByName(const QString &name);
    static bool isProcessRunning(const QString &name);
    static bool isProcessRunning(qint64 pid);
    
    // 终止进程
    static bool killProcess(qint64 pid);
    static bool killProcessByName(const QString &name);
    
    // 清理残留的内核进程
    static void cleanupKernelProcesses();
};

#endif // PROCESSMANAGER_H
