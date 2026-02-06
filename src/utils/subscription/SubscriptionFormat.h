#ifndef SUBSCRIPTIONFORMAT_H
#define SUBSCRIPTIONFORMAT_H
#include <QString>
namespace SubscriptionFormat {
QString formatBytes(qint64 bytes);
QString formatTimestamp(qint64 ms);
QString formatExpireTime(qint64 seconds);
}  // namespace SubscriptionFormat
#endif  // SUBSCRIPTIONFORMAT_H
