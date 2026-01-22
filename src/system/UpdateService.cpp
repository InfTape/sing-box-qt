#include "UpdateService.h"
#include "network/HttpClient.h"
#include "utils/Logger.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

UpdateService::UpdateService(QObject *parent)
    : QObject(parent)
    , m_httpClient(new HttpClient(this))
    , m_currentVersion(QCoreApplication::applicationVersion())
    , m_updateUrl("https://api.github.com/repos/xinggaoya/sing-box-windows/releases/latest")
{
}

UpdateService::~UpdateService()
{
}

QString UpdateService::getCurrentVersion() const
{
    return m_currentVersion;
}

void UpdateService::checkForUpdate()
{
    Logger::info("检查更新...");
    
    m_httpClient->get(m_updateUrl, [this](bool success, const QByteArray &data) {
        if (!success) {
            emit errorOccurred(tr("检查更新失败"));
            return;
        }
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            emit errorOccurred(tr("解析更新信息失败"));
            return;
        }
        
        QJsonObject release = doc.object();
        QString tagName = release["tag_name"].toString();
        QString latestVersion = tagName.startsWith('v') ? tagName.mid(1) : tagName;
        
        // 比较版本
        if (latestVersion > m_currentVersion) {
            UpdateInfo info;
            info.version = latestVersion;
            info.changelog = release["body"].toString();
            info.hasUpdate = true;
            
            // 查找适合当前平台的下载链接
            QJsonArray assets = release["assets"].toArray();
            for (const auto &asset : assets) {
                QJsonObject assetObj = asset.toObject();
                QString name = assetObj["name"].toString();
                
#ifdef Q_OS_WIN
                if (name.endsWith(".exe") || name.endsWith(".msi")) {
                    info.downloadUrl = assetObj["browser_download_url"].toString();
                    info.fileSize = assetObj["size"].toVariant().toLongLong();
                    break;
                }
#endif
            }
            
            Logger::info(QString("发现新版本: %1").arg(latestVersion));
            emit updateAvailable(info);
        } else {
            Logger::info("当前已是最新版本");
            emit noUpdateAvailable();
        }
    });
}

void UpdateService::downloadUpdate(const QString &url, const QString &savePath)
{
    Logger::info(QString("下载更新: %1").arg(url));
    
    m_httpClient->download(url, savePath,
        [this](qint64 received, qint64 total) {
            emit downloadProgress(received, total);
        },
        [this, savePath](bool success, const QByteArray &) {
            if (success) {
                Logger::info("更新下载完成");
                emit downloadFinished(savePath);
            } else {
                emit errorOccurred(tr("下载更新失败"));
            }
        }
    );
}
