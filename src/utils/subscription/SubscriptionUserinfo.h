#ifndef SUBSCRIPTIONUSERINFO_H
#define SUBSCRIPTIONUSERINFO_H

#include <QByteArray>
#include <QJsonObject>

namespace SubscriptionUserinfo {
QJsonObject parseUserinfoHeader(const QByteArray &header);
}

#endif // SUBSCRIPTIONUSERINFO_H
