#ifndef SYSTEMPROXY_H
#define SYSTEMPROXY_H

#include <QObject>
#include <QString>
class SystemProxy : public QObject {
  Q_OBJECT

 public:
  explicit SystemProxy(QObject* parent = nullptr);

  // Set system proxy.
  static bool setProxy(const QString& host, int port);
  static bool clearProxy();

  // Get current proxy status.
  static bool    isProxyEnabled();
  static QString getProxyHost();
  static int     getProxyPort();

  // PAC proxy.
  static bool setPacProxy(const QString& pacUrl);

  // Refresh system proxy settings.
  static void refreshSettings();
};
#endif  // SYSTEMPROXY_H
