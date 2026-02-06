#include "utils/subscription/SubscriptionActions.h"

#include "network/SubscriptionService.h"
namespace SubscriptionActions {
bool useSubscription(SubscriptionService* service, const QString& id) {
  if (!service) return false;
  const QList<SubscriptionInfo> subs        = service->getSubscriptions();
  const int                     activeIndex = service->getActiveIndex();

  int clickedIndex = -1;
  for (int i = 0; i < subs.size(); ++i) {
    if (subs[i].id == id) {
      clickedIndex = i;
      break;
    }
  }
  if (clickedIndex < 0) return false;

  if (clickedIndex == activeIndex) {
    service->refreshSubscription(id, true);
  } else {
    service->setActiveSubscription(id, true);
  }
  return true;
}
void refreshSubscription(SubscriptionService* service, const QString& id, bool applyRuntime) {
  if (!service) return;
  service->refreshSubscription(id, applyRuntime);
}
bool rollbackSubscription(SubscriptionService* service, const QString& id) {
  if (!service) return false;

  const QList<SubscriptionInfo> subs = service->getSubscriptions();
  for (const auto& sub : subs) {
    if (sub.id == id) {
      if (!service->rollbackSubscriptionConfig(sub.configPath)) {
        return false;
      }
      if (service->getActiveIndex() >= 0) {
        service->setActiveSubscription(id, true);
      }
      return true;
    }
  }
  return false;
}
}  // namespace SubscriptionActions
