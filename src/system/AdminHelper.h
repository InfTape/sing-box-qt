#ifndef ADMINHELPER_H
#define ADMINHELPER_H

#include <QObject>
#include <QString>

class AdminHelper : public QObject
{
    Q_OBJECT

public:
    explicit AdminHelper(QObject *parent = nullptr);
    
    // 检查管理员权限
    static bool isAdmin();
    
    // 以管理员身份重启应用
    static bool restartAsAdmin();
    
    // 请求 UAC 提权运行命令
    static bool runAsAdmin(const QString &program, const QStringList &arguments = QStringList());
};

#endif // ADMINHELPER_H
