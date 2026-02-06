#ifndef RULEUTILS_H
#define RULEUTILS_H
#include <QString>
namespace RuleUtils {
QString normalizeRuleTypeKey(const QString& type);
QString displayRuleTypeLabel(const QString& type);
QString normalizeProxyValue(const QString& proxy);
QString displayProxyLabel(const QString& proxy);
bool    isCustomPayload(const QString& payload);
}  // namespace RuleUtils
#endif  // RULEUTILS_H
