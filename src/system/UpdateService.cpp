#include "UpdateService.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "network/HttpClient.h"
#include "utils/Logger.h"
UpdateService::UpdateService(QObject* parent)
    : QObject(parent),
      m_httpClient(new HttpClient(this)),
      m_currentVersion(QCoreApplication::applicationVersion()),
      m_updateUrl(
          "https://api.github.com/repos/xinggaoya/sing-box-windows/releases/"
          "latest") {}
UpdateService::~UpdateService() {}
QString UpdateService::getCurrentVersion() const {
  return m_currentVersion;
}
void UpdateService::checkForUpdate() {
  Logger::info("Checking for updates...");
  m_httpClient->get(m_updateUrl, [this](bool success, const QByteArray& data) {
    if (!success) {
      emit errorOccurred(tr("Update check failed"));
      return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
      emit errorOccurred(tr("Failed to parse update info"));
      return;
    }
    QJsonObject release       = doc.object();
    QString     tagName       = release["tag_name"].toString();
    QString     latestVersion = tagName.startsWith('v') ? tagName.mid(1) : tagName;
    // Compare versions.
    if (latestVersion > m_currentVersion) {
      UpdateInfo info;
      info.version   = latestVersion;
      info.changelog = release["body"].toString();
      info.hasUpdate = true;
      // Find download link for the current platform.
      QJsonArray assets = release["assets"].toArray();
      for (const auto& asset : assets) {
        QJsonObject assetObj = asset.toObject();
        QString     name     = assetObj["name"].toString();
#ifdef Q_OS_WIN
        if (name.endsWith(".exe") || name.endsWith(".msi")) {
          info.downloadUrl = assetObj["browser_download_url"].toString();
          info.fileSize    = assetObj["size"].toVariant().toLongLong();
          break;
        }
#endif
      }
      Logger::info(QString("New version available: %1").arg(latestVersion));
      emit updateAvailable(info);
    } else {
      Logger::info("Already on the latest version");
      emit noUpdateAvailable();
    }
  });
}
void UpdateService::downloadUpdate(const QString& url, const QString& savePath) {
  Logger::info(QString("Downloading update: %1").arg(url));
  m_httpClient->download(
      url, savePath, [this](qint64 received, qint64 total) { emit downloadProgress(received, total); },
      [this, savePath](bool success, const QByteArray&) {
        if (success) {
          Logger::info("Update download completed");
          emit downloadFinished(savePath);
        } else {
          emit errorOccurred(tr("Update download failed"));
        }
      });
}
