#include "KernelPlatform.h"
#include "utils/AppPaths.h"
#include <QStandardPaths>
#include <QObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSysInfo>
#include <QOperatingSystemVersion>
#include <QProcess>
#include <QRegularExpression>
#include <QDirIterator>

namespace KernelPlatform {

QString kernelInstallDir()
{
    return appDataDir();
}

QString detectKernelPath()
{
#ifdef Q_OS_WIN
    const QString kernelName = "sing-box.exe";
#else
    const QString kernelName = "sing-box";
#endif

    const QString dataDir = kernelInstallDir();
    const QString path = dataDir + "/" + kernelName;
    if (QFile::exists(path)) {
        return path;
    }

    return QString();
}

QString queryKernelVersion(const QString &kernelPath)
{
    if (kernelPath.isEmpty() || !QFile::exists(kernelPath)) {
        return QString();
    }

    QProcess process;
    process.start(kernelPath, {"version"});
    if (!process.waitForFinished(3000)) {
        process.kill();
        return QString();
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    QRegularExpression re("(\\d+\\.\\d+\\.\\d+)");
    QRegularExpressionMatch match = re.match(output);
    if (match.hasMatch()) {
        return match.captured(1);
    }

    return output;
}

QString getKernelArch()
{
    const QString arch = QSysInfo::currentCpuArchitecture().toLower();
    if (arch.contains("arm64") || arch.contains("aarch64")) {
        return "arm64";
    }
    if (arch.contains("amd64") || arch.contains("x86_64") || arch.contains("x64") || arch.contains("64")) {
        return "amd64";
    }
    return "386";
}

QString buildKernelFilename(const QString &version)
{
    const QString arch = getKernelArch();
    if (arch.isEmpty()) {
        return QString();
    }

    QString cleanVersion = version;
    if (cleanVersion.startsWith('v')) {
        cleanVersion = cleanVersion.mid(1);
    }

    bool useLegacy = false;
#ifdef Q_OS_WIN
    const QOperatingSystemVersion osVersion = QOperatingSystemVersion::current();
    if (osVersion.majorVersion() > 0 && osVersion.majorVersion() < 10) {
        useLegacy = true;
    }
#endif

    if (useLegacy && (arch == "amd64" || arch == "386")) {
        return QString("sing-box-%1-windows-%2-legacy-windows-7.zip").arg(cleanVersion, arch);
    }

    return QString("sing-box-%1-windows-%2.zip").arg(cleanVersion, arch);
}

QStringList buildDownloadUrls(const QString &version, const QString &filename)
{
    QString tag = version;
    if (!tag.startsWith('v')) {
        tag = "v" + tag;
    }

    const QString base = QString("https://github.com/SagerNet/sing-box/releases/download/%1/%2").arg(tag, filename);

    QStringList urls;
    urls << base;
    urls << "https://ghproxy.com/" + base;
    urls << "https://mirror.ghproxy.com/" + base;
    urls << "https://ghproxy.net/" + base;
    return urls;
}

QString findExecutableInDir(const QString &dirPath, const QString &exeName)
{
    QDirIterator it(dirPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (QFileInfo(path).fileName().compare(exeName, Qt::CaseInsensitive) == 0) {
            return path;
        }
    }
    return QString();
}

bool extractZipArchive(const QString &zipPath, const QString &destDir, QString *errorMessage)
{
#ifdef Q_OS_WIN
    QDir dest(destDir);
    if (dest.exists()) {
        dest.removeRecursively();
    }
    QDir().mkpath(destDir);

    const QString command = QString("Expand-Archive -Force -LiteralPath \"%1\" -DestinationPath \"%2\"")
        .arg(zipPath, destDir);

    QProcess proc;
    proc.start("powershell", {"-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", command});
    if (!proc.waitForFinished(300000)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Extraction timed out");
        }
        proc.kill();
        return false;
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            if (errorMessage->isEmpty()) {
                *errorMessage = QObject::tr("Extraction failed");
            }
        }
        return false;
    }

    return true;
#else
    Q_UNUSED(zipPath)
    Q_UNUSED(destDir)
    if (errorMessage) {
        *errorMessage = QObject::tr("Extraction not supported on this platform");
    }
    return false;
#endif
}

} // namespace KernelPlatform
