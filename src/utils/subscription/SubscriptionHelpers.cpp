#include "utils/subscription/SubscriptionHelpers.h"
#include <QJsonArray>
#include <QJsonDocument>
#include "network/SubscriptionService.h"

namespace SubscriptionHelpers {
bool isSingleManualNode(const SubscriptionInfo& info, QJsonObject* outNode) {
  if (!info.isManual || info.useOriginalConfig) return false;
  QJsonDocument doc = QJsonDocument::fromJson(info.manualContent.toUtf8());
  if (doc.isArray()) {
    QJsonArray arr = doc.array();
    if (arr.count() == 1 && arr[0].isObject()) {
      if (outNode) *outNode = arr[0].toObject();
      return true;
    }
  } else if (doc.isObject()) {
    QJsonObject obj = doc.object();
    if (obj.contains("type") && obj.contains("server")) {
      if (outNode) *outNode = obj;
      return true;
    }
  }
  return false;
}
}  // namespace SubscriptionHelpers
