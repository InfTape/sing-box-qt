#include "utils/subscription/SubscriptionUserinfo.h"
#include <QStringList>

namespace SubscriptionUserinfo {

QJsonObject parseUserinfoHeader(const QByteArray &header)
{
    QJsonObject info;
    if (header.isEmpty()) {
        return info;
    }
    const QString raw = QString::fromUtf8(header).trimmed();
    const QStringList segments = raw.split(';', Qt::SkipEmptyParts);
    for (const QString &segment : segments) {
        const QStringList pair = segment.trimmed().split('=', Qt::SkipEmptyParts);
        if (pair.size() != 2) continue;
        const QString key = pair[0].trimmed().toLower();
        const qint64 value = pair[1].trimmed().toLongLong();
        if (value < 0) continue;
        if (key == "upload") info["upload"] = value;
        if (key == "download") info["download"] = value;
        if (key == "total") info["total"] = value;
        if (key == "expire") info["expire"] = value;
    }
    return info;
}

} // namespace SubscriptionUserinfo
