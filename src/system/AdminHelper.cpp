#include "AdminHelper.h"
#include "utils/Logger.h"
#include <QCoreApplication>
#include <QProcess>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
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
    // Unix: check if running as root.
    return geteuid() == 0;
#endif
}

bool AdminHelper::restartAsAdmin()
{
    QString program = QCoreApplication::applicationFilePath();
    QStringList args = QCoreApplication::arguments();
    args.removeFirst(); // Remove program path.

    if (runAsAdmin(program, args)) {
        // Exit current instance.
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
        Logger::info("Admin elevation requested");
        return true;
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {
            Logger::warn("User canceled UAC request");
        } else {
            Logger::error(QString("Admin elevation failed, error code: %1").arg(error));
        }
        return false;
    }
#else
    QString helper = QStandardPaths::findExecutable("pkexec");
    QStringList args = arguments;
    if (!helper.isEmpty()) {
        args.prepend(program);
    } else {
        helper = QStandardPaths::findExecutable("doas");
        if (!helper.isEmpty()) {
            args.prepend(program);
        } else {
            helper = QStandardPaths::findExecutable("sudo");
            if (!helper.isEmpty()) {
                args.prepend(program);
            }
        }
    }

    if (helper.isEmpty()) {
        Logger::warn("No privilege elevation helper found (pkexec/doas/sudo)");
        return false;
    }

    if (QProcess::startDetached(helper, args)) {
        Logger::info("Admin elevation requested");
        return true;
    }

    Logger::error("Admin elevation failed to start");
    return false;
#endif
}
