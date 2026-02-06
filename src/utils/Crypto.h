#ifndef CRYPTO_H
#define CRYPTO_H
#include <QByteArray>
#include <QString>

class Crypto {
 public:
  // Base64 encode/decode.
  static QString    base64Encode(const QByteArray& data);
  static QByteArray base64Decode(const QString& base64);
  // SHA256 hash.
  static QString sha256(const QByteArray& data);
  static QString sha256(const QString& text);
  // UUID generation.
  static QString generateUUID();
  // URL-safe Base64.
  static QString    base64UrlEncode(const QByteArray& data);
  static QByteArray base64UrlDecode(const QString& base64);
};
#endif  // CRYPTO_H
