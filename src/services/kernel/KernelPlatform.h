#ifndef KERNELPLATFORM_H
#define KERNELPLATFORM_H
#include <QString>
#include <QStringList>

namespace KernelPlatform {
QString     kernelInstallDir();
QString     detectKernelPath();
QString     queryKernelVersion(const QString& kernelPath);
QString     getKernelArch();
QString     buildKernelFilename(const QString& version);
QStringList buildDownloadUrls(const QString& version, const QString& filename);
QString     findExecutableInDir(const QString& dirPath, const QString& exeName);
bool        extractZipArchive(const QString& zipPath, const QString& destDir, QString* errorMessage);
}  // namespace KernelPlatform
#endif  // KERNELPLATFORM_H
