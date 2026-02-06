#include "HttpClient.h"
#include <QFile>
#include <QTimer>
#include "utils/Logger.h"

HttpClient::HttpClient(QObject* parent)
    : QObject(parent),
      m_manager(new QNetworkAccessManager(this)),
      m_timeout(30000) {}

HttpClient::~HttpClient() {}

void HttpClient::setAuthToken(const QString& token) {
  m_authToken = token;
}

void HttpClient::setTimeout(int msecs) {
  m_timeout = msecs;
}

QNetworkRequest HttpClient::createRequest(const QString& url) {
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  request.setTransferTimeout(m_timeout);
  request.setRawHeader("User-Agent", "sing-box-qt");
  request.setRawHeader("Accept", "application/vnd.github+json");
  if (!m_authToken.isEmpty()) {
    request.setRawHeader("Authorization",
                         QString("Bearer %1").arg(m_authToken).toUtf8());
  }
  return request;
}

void HttpClient::handleReply(QNetworkReply* reply, Callback callback) {
  connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
    bool       success = (reply->error() == QNetworkReply::NoError);
    QByteArray data    = reply->readAll();
    if (!success) {
      Logger::warn(
          QString("HTTP request failed: %1").arg(reply->errorString()));
    }
    callback(success, data);
    reply->deleteLater();
  });
}

void HttpClient::get(const QString& url, Callback callback) {
  QNetworkRequest request = createRequest(url);
  QNetworkReply*  reply   = m_manager->get(request);
  handleReply(reply, callback);
}

void HttpClient::post(const QString&    url,
                      const QByteArray& data,
                      Callback          callback) {
  QNetworkRequest request = createRequest(url);
  QNetworkReply*  reply   = m_manager->post(request, data);
  handleReply(reply, callback);
}

void HttpClient::put(const QString&    url,
                     const QByteArray& data,
                     Callback          callback) {
  QNetworkRequest request = createRequest(url);
  QNetworkReply*  reply   = m_manager->put(request, data);
  handleReply(reply, callback);
}

void HttpClient::del(const QString& url, Callback callback) {
  QNetworkRequest request = createRequest(url);
  QNetworkReply*  reply   = m_manager->deleteResource(request);
  handleReply(reply, callback);
}

void HttpClient::download(const QString&                      url,
                          const QString&                      savePath,
                          std::function<void(qint64, qint64)> progressCallback,
                          Callback                            callback) {
  QNetworkRequest request = createRequest(url);
  QNetworkReply*  reply   = m_manager->get(request);
  if (progressCallback) {
    connect(reply,
            &QNetworkReply::downloadProgress,
            this,
            [progressCallback](qint64 received, qint64 total) {
              progressCallback(received, total);
            });
  }
  connect(reply, &QNetworkReply::finished, this, [reply, savePath, callback]() {
    if (reply->error() != QNetworkReply::NoError) {
      Logger::error(QString("Download failed: %1").arg(reply->errorString()));
      callback(false, QByteArray());
      reply->deleteLater();
      return;
    }
    QFile file(savePath);
    if (file.open(QIODevice::WriteOnly)) {
      file.write(reply->readAll());
      file.close();
      Logger::info(QString("Download completed: %1").arg(savePath));
      callback(true, QByteArray());
    } else {
      Logger::error(QString("Failed to write file: %1").arg(savePath));
      callback(false, QByteArray());
    }
    reply->deleteLater();
  });
}
