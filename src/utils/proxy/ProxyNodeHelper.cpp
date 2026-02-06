#include "utils/proxy/ProxyNodeHelper.h"

QString ProxyNodeHelper::delayStateFromText(const QString& delayText) {
  if (delayText.isEmpty() || delayText == "...") {
    return "loading";
  }
  QString delayStr = delayText;
  QString trimmed  = delayStr;
  trimmed.replace(" ms", "");
  bool      ok    = false;
  const int delay = trimmed.toInt(&ok);
  if (!ok) {
    return QString();
  }
  if (delay <= 0) {
    return "bad";
  }
  if (delay < 100) {
    return "ok";
  }
  if (delay < 300) {
    return "warn";
  }
  return "bad";
}
