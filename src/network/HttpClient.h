#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <functional>
class HttpClient : public QObject {
  Q_OBJECT

 public:
  using Callback = std::function<void(bool success, const QByteArray& data)>;

  explicit HttpClient(QObject* parent = nullptr);
  ~HttpClient();

  void setAuthToken(const QString& token);
  void setTimeout(int msecs);

  void get(const QString& url, Callback callback);
  void post(const QString& url, const QByteArray& data, Callback callback);
  void put(const QString& url, const QByteArray& data, Callback callback);
  void del(const QString& url, Callback callback);

  // Download file.
  void download(const QString& url, const QString& savePath, std::function<void(qint64, qint64)> progressCallback,
                Callback callback);

 private:
  QNetworkRequest createRequest(const QString& url);
  void            handleReply(QNetworkReply* reply, Callback callback);

  QNetworkAccessManager* m_manager;
  QString                m_authToken;
  int                    m_timeout;
};
#endif  // HTTPCLIENT_H
