#ifndef CRYPTO_H
#define CRYPTO_H

#include <QString>
#include <QByteArray>

class Crypto
{
public:
    // Base64 编解码
    static QString base64Encode(const QByteArray &data);
    static QByteArray base64Decode(const QString &base64);
    
    // SHA256 哈希
    static QString sha256(const QByteArray &data);
    static QString sha256(const QString &text);
    
    // UUID 生成
    static QString generateUUID();
    
    // URL 安全的 Base64
    static QString base64UrlEncode(const QByteArray &data);
    static QByteArray base64UrlDecode(const QString &base64);
};

#endif // CRYPTO_H
