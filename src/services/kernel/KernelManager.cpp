#include "services/kernel/KernelManager.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSet>
#include <QSysInfo>
#include "core/ProcessManager.h"
#include "network/HttpClient.h"
#include "services/kernel/KernelPlatform.h"
#include "utils/GitHubMirror.h"
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
  return lower.contains("rc") || lower.contains("beta") ||
         lower.contains("alpha");
}

QStringList extractVersionsFromReleasesHtml(const QByteArray& htmlData) {
  const QString html = QString::fromUtf8(htmlData);
  QRegularExpression re(
      R"(\/SagerNet\/sing-box\/releases\/tag\/v?(\d+\.\d+\.\d+(?:-[A-Za-z0-9._-]+)?))");
  QStringList versions;
  QSet<QString> seen;
  auto matchIt = re.globalMatch(html);
  while (matchIt.hasNext()) {
    const QString tag = matchIt.next().captured(1).trimmed();
    if (tag.isEmpty() || isPreReleaseTag(tag) || seen.contains(tag)) {
      continue;
    }
    seen.insert(tag);
    versions.append(tag);
  }
  return versions;
}
}  // namespace

KernelManager::KernelManager(QObject* parent)
    : QObject(parent), m_httpClient(new HttpClient(this)) {
  if (m_httpClient) {
    QString token = qEnvironmentVariable("GITHUB_TOKEN").trimmed();
    if (token.isEmpty()) {
      token = qEnvironmentVariable("GH_TOKEN").trimmed();
    }
    if (!token.isEmpty()) {
      m_httpClient->setAuthToken(token);
    }
  }
}

QString KernelManager::normalizedLatest(const QString& rawTag) const {
  return normalizeVersionTag(rawTag);
}

QStringList KernelManager::kernelReleasesApiUrls() const {
  return GitHubMirror::buildUrls(
      "https://api.github.com/repos/SagerNet/sing-box/releases");
}

QStringList KernelManager::kernelReleasesPageUrls() const {
  return GitHubMirror::buildUrls("https://github.com/SagerNet/sing-box/releases");
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
  const QStringList pageUrls = kernelReleasesPageUrls();
  auto tryPageFetch = std::make_shared<std::function<void(int)>>();
  *tryPageFetch = [this, pageUrls, tryPageFetch](int index) {
    if (index >= pageUrls.size()) {
      Logger::warn(tr("Failed to fetch kernel version list"));
      emit releasesReady(QStringList(), QString());
      return;
    }
    const QString url = pageUrls.at(index);
    m_httpClient->get(
        url,
        [this, pageUrls, index, tryPageFetch](bool success,
                                               const QByteArray& data) {
          if (!success) {
            (*tryPageFetch)(index + 1);
            return;
          }
          const QStringList versions = extractVersionsFromReleasesHtml(data);
          if (versions.isEmpty()) {
            (*tryPageFetch)(index + 1);
            return;
          }
          m_latestKernelVersion = versions.first();
          emit releasesReady(versions, m_latestKernelVersion);
        });
  };

  auto tryApiFetch = std::make_shared<std::function<void(int)>>();
  *tryApiFetch = [this, apiUrls, tryApiFetch, tryPageFetch](int index) {
    if (index >= apiUrls.size()) {
      (*tryPageFetch)(0);
      return;
    }
    const QString url = apiUrls.at(index);
    m_httpClient->get(
        url,
        [this, apiUrls, index, tryApiFetch, tryPageFetch](
            bool success, const QByteArray& data) {
          if (!success) {
            (*tryApiFetch)(index + 1);
            return;
          }
          QJsonDocument doc = QJsonDocument::fromJson(data);
          if (!doc.isArray()) {
            (*tryApiFetch)(index + 1);
            return;
          }
          QStringList      versions;
          const QJsonArray arr = doc.array();
          for (const QJsonValue& item : arr) {
            const QJsonObject obj = item.toObject();
            const bool prerelease = obj.value("prerelease").toBool(false);
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
            (*tryApiFetch)(index + 1);
            return;
          }
          m_latestKernelVersion = versions.first();
          emit releasesReady(versions, m_latestKernelVersion);
        });
  };
  (*tryApiFetch)(0);
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
  const QString tempDir =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
      "/sing-box";
  QDir().mkpath(tempDir);
  const QString     zipPath    = tempDir + "/" + filename;
  const QString     extractDir = tempDir + "/extract-" + targetVersion;
  const QStringList urls =
      KernelPlatform::buildDownloadUrls(targetVersion, filename);
  if (urls.isEmpty()) {
    emit finished(false, tr("Download URL is empty"));
    return;
  }
  m_isDownloading = true;
  emit statusChanged(tr("Preparing to download kernel..."));
  tryDownloadUrl(0, urls, zipPath, extractDir, targetVersion);
}

void KernelManager::tryDownloadUrl(int                index,
                                   const QStringList& urls,
                                   const QString&     savePath,
                                   const QString&     extractDir,
                                   const QString&     version) {
  if (index >= urls.size()) {
    m_isDownloading = false;
    emit finished(false, tr("Failed to download kernel from mirror"));
    return;
  }
  const QString url = urls.at(index);
  emit          statusChanged(tr("Downloading: %1").arg(url));
  m_httpClient->download(
      url,
      savePath,
      [this](qint64 received, qint64 total) {
        if (total > 0) {
          int  percent = static_cast<int>((received * 100) / total);
          emit downloadProgress(percent);
        }
      },
      [this, urls, index, savePath, extractDir, version](bool success,
                                                         const QByteArray&) {
        if (!success) {
          tryDownloadUrl(index + 1, urls, savePath, extractDir, version);
          return;
        }
        QString errorMessage;
        if (!KernelPlatform::extractZipArchive(
                savePath, extractDir, &errorMessage)) {
          m_isDownloading = false;
          emit finished(false, tr("Extract failed: %1").arg(errorMessage));
          return;
        }
        QString exeName =
            QSysInfo::productType() == "windows" ? "sing-box.exe" : "sing-box";
        const QString foundExe =
            KernelPlatform::findExecutableInDir(extractDir, exeName);
        if (foundExe.isEmpty()) {
          m_isDownloading = false;
          emit finished(false, tr("sing-box executable not found in archive"));
          return;
        }
        const QString dataDir = KernelPlatform::kernelInstallDir();
        QDir().mkpath(dataDir);
        const QString destPath   = dataDir + "/" + exeName;
        ProcessManager::killProcessByPath(destPath);
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
        refreshInstalledInfo();
        emit finished(true, tr("Kernel downloaded and installed successfully"));
      });
}
