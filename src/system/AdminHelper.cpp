#include "AdminHelper.h"
#include <QCoreApplication>
#include <QRegularExpression>
#include "utils/Logger.h"
#ifdef Q_OS_WIN
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on
#else
#include <unistd.h>
#endif

namespace {
constexpr auto kReplaceExistingInstanceArg = "--replace-existing-instance";

#ifdef Q_OS_WIN
QString quoteWindowsArgument(const QString& arg) {
  if (!arg.contains(QRegularExpression(QStringLiteral("[\\s\"]")))) {
    return arg;
  }
  QString quoted     = QStringLiteral("\"");
  int     slashCount = 0;
  for (const QChar ch : arg) {
    if (ch == '\\') {
      slashCount += 1;
      continue;
    }
    if (ch == '"') {
      quoted += QString(slashCount * 2 + 1, '\\');
      quoted += ch;
      slashCount = 0;
      continue;
    }
    if (slashCount > 0) {
      quoted += QString(slashCount, '\\');
      slashCount = 0;
    }
    quoted += ch;
  }
  if (slashCount > 0) {
    quoted += QString(slashCount * 2, '\\');
  }
  quoted += QStringLiteral("\"");
  return quoted;
}

QString buildWindowsArguments(const QStringList& arguments) {
  QStringList quoted;
  quoted.reserve(arguments.size());
  for (const QString& arg : arguments) {
    quoted.append(quoteWindowsArgument(arg));
  }
  return quoted.join(' ');
}
#endif
}  // namespace

AdminHelper::AdminHelper(QObject* parent) : QObject(parent) {}

bool AdminHelper::isAdmin() {
#ifdef Q_OS_WIN
  BOOL                     isAdmin     = FALSE;
  PSID                     adminGroup  = nullptr;
  SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
  if (AllocateAndInitializeSid(&ntAuthority,
                               2,
                               SECURITY_BUILTIN_DOMAIN_RID,
                               DOMAIN_ALIAS_RID_ADMINS,
                               0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               &adminGroup)) {
    CheckTokenMembership(nullptr, adminGroup, &isAdmin);
    FreeSid(adminGroup);
  }
  return isAdmin;
#else
  // Unix: check if running as root.
  return getuid() == 0;
#endif
}

bool AdminHelper::restartAsAdmin() {
  QString     program = QCoreApplication::applicationFilePath();
  QStringList args    = QCoreApplication::arguments();
  args.removeFirst();  // Remove program path.
  const QString replaceArg = QString::fromLatin1(kReplaceExistingInstanceArg);
  if (!args.contains(replaceArg)) {
    args.append(replaceArg);
  }
  if (runAsAdmin(program, args)) {
    // Exit current instance.
    QCoreApplication::quit();
    return true;
  }
  return false;
}

bool AdminHelper::runAsAdmin(const QString&     program,
                             const QStringList& arguments) {
#ifdef Q_OS_WIN
  QString           args = buildWindowsArguments(arguments);
  SHELLEXECUTEINFOW sei  = {};
  sei.cbSize             = sizeof(sei);
  sei.lpVerb             = L"runas";
  sei.lpFile             = reinterpret_cast<LPCWSTR>(program.utf16());
  sei.lpParameters       = reinterpret_cast<LPCWSTR>(args.utf16());
  sei.nShow              = SW_SHOWNORMAL;
  sei.fMask              = SEE_MASK_NOCLOSEPROCESS;
  if (ShellExecuteExW(&sei)) {
    if (sei.hProcess) {
      CloseHandle(sei.hProcess);
    }
    Logger::info("Admin elevation requested");
    return true;
  } else {
    DWORD error = GetLastError();
    if (error == ERROR_CANCELLED) {
      Logger::warn("User canceled UAC request");
    } else {
      Logger::error(
          QString("Admin elevation failed, error code: %1").arg(error));
    }
    return false;
  }
#else
  // TODO(yourdaddy): use pkexec or similar on Linux/macOS.
  Q_UNUSED(program)
  Q_UNUSED(arguments)
  return false;
#endif
}

bool AdminHelper::runAsAdminAndWait(const QString&     program,
                                    const QStringList& arguments,
                                    int                timeoutMs) {
#ifdef Q_OS_WIN
  QString           args = buildWindowsArguments(arguments);
  SHELLEXECUTEINFOW sei  = {};
  sei.cbSize             = sizeof(sei);
  sei.lpVerb             = L"runas";
  sei.lpFile             = reinterpret_cast<LPCWSTR>(program.utf16());
  sei.lpParameters       = reinterpret_cast<LPCWSTR>(args.utf16());
  sei.nShow              = SW_HIDE;
  sei.fMask              = SEE_MASK_NOCLOSEPROCESS;
  if (!ShellExecuteExW(&sei)) {
    const DWORD error = GetLastError();
    if (error == ERROR_CANCELLED) {
      Logger::warn("User canceled UAC request");
    } else {
      Logger::error(
          QString("Admin elevation failed, error code: %1").arg(error));
    }
    return false;
  }
  if (!sei.hProcess) {
    return true;
  }
  const DWORD waitResult = WaitForSingleObject(
      sei.hProcess, timeoutMs < 0 ? INFINITE : static_cast<DWORD>(timeoutMs));
  if (waitResult != WAIT_OBJECT_0) {
    CloseHandle(sei.hProcess);
    return false;
  }
  DWORD exitCode = 1;
  GetExitCodeProcess(sei.hProcess, &exitCode);
  CloseHandle(sei.hProcess);
  return exitCode == 0;
#else
  Q_UNUSED(program)
  Q_UNUSED(arguments)
  Q_UNUSED(timeoutMs)
  return false;
#endif
}
