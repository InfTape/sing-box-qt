#include "AdminHelper.h"
#include "utils/Logger.h"
#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

AdminHelper::AdminHelper(QObject *parent)
    : QObject(parent)
{
}

bool AdminHelper::isAdmin()
{
#ifdef Q_OS_WIN
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2,
            SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    
    return isAdmin;
#else
    // Unix: 检查是否为 root
    return getuid() == 0;
#endif
}

bool AdminHelper::restartAsAdmin()
{
    QString program = QCoreApplication::applicationFilePath();
    QStringList args = QCoreApplication::arguments();
    args.removeFirst(); // 移除程序路径
    
    if (runAsAdmin(program, args)) {
        // 退出当前实例
        QCoreApplication::quit();
        return true;
    }
    
    return false;
}

bool AdminHelper::runAsAdmin(const QString &program, const QStringList &arguments)
{
#ifdef Q_OS_WIN
    QString args = arguments.join(' ');
    
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = L"runas";
    sei.lpFile = reinterpret_cast<LPCWSTR>(program.utf16());
    sei.lpParameters = reinterpret_cast<LPCWSTR>(args.utf16());
    sei.nShow = SW_SHOWNORMAL;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    
    if (ShellExecuteExW(&sei)) {
        Logger::info("已请求管理员权限");
        return true;
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {
            Logger::warn("用户取消了 UAC 请求");
        } else {
            Logger::error(QString("请求管理员权限失败，错误码: %1").arg(error));
        }
        return false;
    }
#else
    // TODO: Linux/macOS 使用 pkexec 或类似工具
    Q_UNUSED(program)
    Q_UNUSED(arguments)
    return false;
#endif
}
