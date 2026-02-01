#pragma once

#include <QJsonObject>

struct SubscriptionInfo;
namespace SubscriptionHelpers {
bool isSingleManualNode(const SubscriptionInfo& info, QJsonObject* outNode);
}
