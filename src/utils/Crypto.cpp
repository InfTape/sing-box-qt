#include "Crypto.h"

#include <QCryptographicHash>
#include <QUuid>
QString Crypto::base64Encode(const QByteArray& data) {
  return QString::fromLatin1(data.toBase64());
}
QByteArray Crypto::base64Decode(const QString& base64) {
  return QByteArray::fromBase64(base64.toLatin1());
}
QString Crypto::sha256(const QByteArray& data) {
  QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
  return QString::fromLatin1(hash.toHex());
}
QString Crypto::sha256(const QString& text) {
  return sha256(text.toUtf8());
}
QString Crypto::generateUUID() {
  return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
QString Crypto::base64UrlEncode(const QByteArray& data) {
  QString base64 = QString::fromLatin1(data.toBase64());
  base64.replace('+', '-');
  base64.replace('/', '_');
  base64.remove('=');
  return base64;
}
QByteArray Crypto::base64UrlDecode(const QString& base64) {
  QString data = base64;
  data.replace('-', '+');
  data.replace('_', '/');

  // Fix padding.
  int padding = (4 - data.length() % 4) % 4;
  data.append(QString(padding, '='));

  return QByteArray::fromBase64(data.toLatin1());
}
