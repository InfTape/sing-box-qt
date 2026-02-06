#include "services/kernel/KernelManager.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QSysInfo>
#include "core/ProcessManager.h"
#include "network/HttpClient.h"
#include "services/kernel/KernelPlatform.h"
#include "utils/Logger.h"

namespace {
QString normalizeVersionTag(const QString& raw) {
  QString ver = raw.trimmed();
  if (ver.startsWith('v')) {
    ver = ver.mid(1);
  }
  return ver;
}

bool isPreReleaseTag(const QString& tag) {
  const QString lower = tag.toLower();
  return lower.contains("rc") || lower.contains("beta") || lower.contains("alpha");
}
}  // namespace

KernelManager::KernelManager(QObject* parent) : QObject(parent), m_httpClient(new HttpClient(this)) {}

QString KernelManager::normalizedLatest(const QString& rawTag) const {
  return normalizeVersionTag(rawTag);
}

QStringList KernelManager::latestKernelApiUrls() const {
  return {
      "https://api.github.com/repos/SagerNet/sing-box/releases/latest",
      "https://v6.gh-proxy.com/https://api.github.com/repos/SagerNet/sing-box/"
      "releases/latest",
      "https://gh-proxy.com/https://api.github.com/repos/SagerNet/sing-box/"
      "releases/latest",
      "https://ghfast.top/https://api.github.com/repos/SagerNet/sing-box/"
      "releases/latest",
  };
}

QStringList KernelManager::kernelReleasesApiUrls() const {
  return {
      "https://api.github.com/repos/SagerNet/sing-box/releases",
      "https://v6.gh-proxy.com/https://api.github.com/repos/SagerNet/sing-box/"
      "releases",
      "https://gh-proxy.com/https://api.github.com/repos/SagerNet/sing-box/"
      "releases",
      "https://ghfast.top/https://api.github.com/repos/SagerNet/sing-box/"
      "releases",
  };
}

void KernelManager::refreshInstalledInfo() {
  const QString kernelPath = KernelPlatform::detectKernelPath();
  const QString version    = KernelPlatform::queryKernelVersion(kernelPath);
  emit          installedInfoReady(kernelPath, version);
}

void KernelManager::fetchReleaseList() {
  if (!m_httpClient) {
    return;
  }
  const QStringList apiUrls  = kernelReleasesApiUrls();
  auto              tryFetch = std::make_shared<std::function<void(int)>>();
  *tryFetch                  = [this, apiUrls, tryFetch](int index) {
    if (index >= apiUrls.size()) {
      Logger::warn(tr("Failed to fetch kernel version list"));
      emit releasesReady(QStringList(), QString());
      return;
    }
    const QString url = apiUrls.at(index);
    m_httpClient->get(url, [this, apiUrls, index, tryFetch](bool success, const QByteArray& data) {
      if (!success) {
        (*tryFetch)(index + 1);
        return;
      }
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (!doc.isArray()) {
        (*tryFetch)(index + 1);
        return;
      }
      QStringList      versions;
      const QJsonArray arr = doc.array();
      for (const QJsonValue& item : arr) {
        const QJsonObject obj        = item.toObject();
        const bool        prerelease = obj.value("prerelease").toBool(false);
        if (prerelease) {
          continue;
        }
        const QString tag = obj.value("tag_name").toString();
        if (tag.isEmpty() || isPreReleaseTag(tag)) {
          continue;
        }
        versions.append(normalizeVersionTag(tag));
      }
      if (versions.isEmpty()) {
        (*tryFetch)(index + 1);
        return;
      }
      m_latestKernelVersion = versions.first();
      emit releasesReady(versions, m_latestKernelVersion);
    });
  };
  (*tryFetch)(0);
}

void KernelManager::checkLatest() {
  if (!m_httpClient) {
    return;
  }
  const QString     installedVersion = KernelPlatform::queryKernelVersion(KernelPlatform::detectKernelPath());
  const QStringList apiUrls          = latestKernelApiUrls();
  auto              tryFetch         = std::make_shared<std::function<void(int)>>();
  *tryFetch                          = [this, apiUrls, installedVersion, tryFetch](int index) {
    if (index >= apiUrls.size()) {
      emit finished(false, tr("Failed to fetch kernel versions. Please try again."));
      return;
    }
    const QString url = apiUrls.at(index);
    m_httpClient->get(url, [this, apiUrls, installedVersion, index, tryFetch](bool success, const QByteArray& data) {
      if (!success) {
        (*tryFetch)(index + 1);
        return;
      }
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (!doc.isObject()) {
        (*tryFetch)(index + 1);
        return;
      }
      const QJsonObject obj    = doc.object();
      QString           latest = normalizeVersionTag(obj.value("tag_name").toString());
      if (latest.isEmpty()) {
        (*tryFetch)(index + 1);
        return;
      }
      emit latestReady(latest, normalizeVersionTag(installedVersion));
    });
  };
  (*tryFetch)(0);
}

void KernelManager::downloadAndInstall(const QString& versionOrEmpty) {
  if (m_isDownloading) {
    return;
  }
  QString targetVersion = versionOrEmpty.trimmed();
  if (targetVersion.isEmpty()) {
    targetVersion = m_latestKernelVersion.trimmed();
  }
  if (targetVersion.isEmpty()) {
    emit finished(false, tr("Please check the kernel version list first"));
    return;
  }
  const QString filename = KernelPlatform::buildKernelFilename(targetVersion);
  if (filename.isEmpty()) {
    emit finished(false, tr("Unsupported system architecture"));
    return;
  }
  const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/sing-box";
  QDir().mkpath(tempDir);
  const QString     zipPath    = tempDir + "/" + filename;
  const QString     extractDir = tempDir + "/extract-" + targetVersion;
  const QStringList urls       = KernelPlatform::buildDownloadUrls(targetVersion, filename);
  if (urls.isEmpty()) {
    emit finished(false, tr("Download URL is empty"));
    return;
  }
  m_isDownloading = true;
  emit statusChanged(tr("Preparing to download kernel..."));
  tryDownloadUrl(0, urls, zipPath, extractDir, targetVersion);
}

void KernelManager::tryDownloadUrl(int index, const QStringList& urls, const QString& savePath,
                                   const QString& extractDir, const QString& version) {
  if (index >= urls.size()) {
    m_isDownloading = false;
    emit finished(false, tr("Failed to download kernel from mirror"));
    return;
  }
  const QString url = urls.at(index);
  emit          statusChanged(tr("Downloading: %1").arg(url));
  m_httpClient->download(
      url, savePath,
      [this](qint64 received, qint64 total) {
        if (total > 0) {
          int  percent = static_cast<int>((received * 100) / total);
          emit downloadProgress(percent);
        }
      },
      [this, urls, index, savePath, extractDir, version](bool success, const QByteArray&) {
        if (!success) {
          tryDownloadUrl(index + 1, urls, savePath, extractDir, version);
          return;
        }
        QString errorMessage;
        if (!KernelPlatform::extractZipArchive(savePath, extractDir, &errorMessage)) {
          m_isDownloading = false;
          emit finished(false, tr("Extract failed: %1").arg(errorMessage));
          return;
        }
        QString       exeName  = QSysInfo::productType() == "windows" ? "sing-box.exe" : "sing-box";
        const QString foundExe = KernelPlatform::findExecutableInDir(extractDir, exeName);
        if (foundExe.isEmpty()) {
          m_isDownloading = false;
          emit finished(false, tr("sing-box executable not found in archive"));
          return;
        }
        ProcessManager::killProcessByName("sing-box.exe");
        const QString dataDir = KernelPlatform::kernelInstallDir();
        QDir().mkpath(dataDir);
        const QString destPath   = dataDir + "/" + exeName;
        const QString backupPath = destPath + ".old";
        if (QFile::exists(destPath)) {
          QFile::remove(backupPath);
          QFile::rename(destPath, backupPath);
        }
        if (!QFile::copy(foundExe, destPath)) {
          m_isDownloading = false;
          emit finished(false, tr("Install failed: cannot write kernel file"));
          return;
        }
        m_isDownloading = false;
        emit statusChanged(tr("Download complete"));
        emit finished(true, tr("Kernel downloaded and installed successfully"));
        refreshInstalledInfo();
      });
}
