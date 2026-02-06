#ifndef KERNELMANAGER_H
#define KERNELMANAGER_H
#include <QObject>
#include <QStringList>
class HttpClient;
class KernelManager : public QObject {
  Q_OBJECT
 public:
  explicit KernelManager(QObject* parent = nullptr);
  void refreshInstalledInfo();
  void fetchReleaseList();
  void checkLatest();
  void downloadAndInstall(const QString& versionOrEmpty);
 signals:
  void installedInfoReady(const QString& path, const QString& version);
  void releasesReady(const QStringList& versions, const QString& latest);
  void latestReady(const QString& latest, const QString& installed);
  void downloadProgress(int percent);
  void statusChanged(const QString& status);
  void finished(bool ok, const QString& message);

 private:
  void        tryDownloadUrl(int index, const QStringList& urls, const QString& savePath, const QString& extractDir,
                             const QString& version);
  QString     normalizedLatest(const QString& rawTag) const;
  QStringList latestKernelApiUrls() const;
  QStringList kernelReleasesApiUrls() const;
  HttpClient* m_httpClient;
  QString     m_latestKernelVersion;
  bool        m_isDownloading = false;
};
#endif  // KERNELMANAGER_H
