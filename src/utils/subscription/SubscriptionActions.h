#pragma once
#include <QString>
class SubscriptionService;
namespace SubscriptionActions {
bool useSubscription(SubscriptionService* service, const QString& id);
void refreshSubscription(SubscriptionService* service, const QString& id, bool applyRuntime);
bool rollbackSubscription(SubscriptionService* service, const QString& id);
}  // namespace SubscriptionActions
