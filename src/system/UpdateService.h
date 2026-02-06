#ifndef UPDATESERVICE_H
#define UPDATESERVICE_H
#include <QObject>
#include <QString>
class HttpClient;
struct UpdateInfo {
  QString version;
  QString downloadUrl;
  QString changelog;
  qint64  fileSize;
  bool    hasUpdate;
};
class UpdateService : public QObject {
  Q_OBJECT
 public:
  explicit UpdateService(QObject* parent = nullptr);
  ~UpdateService();
  void    checkForUpdate();
  void    downloadUpdate(const QString& url, const QString& savePath);
  QString getCurrentVersion() const;
 signals:
  void updateAvailable(const UpdateInfo& info);
  void noUpdateAvailable();
  void downloadProgress(qint64 received, qint64 total);
  void downloadFinished(const QString& filePath);
  void errorOccurred(const QString& error);

 private:
  HttpClient* m_httpClient;
  QString     m_currentVersion;
  QString     m_updateUrl;
};
#endif  // UPDATESERVICE_H
