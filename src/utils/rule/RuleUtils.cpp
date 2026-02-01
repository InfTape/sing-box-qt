#include "utils/rule/RuleUtils.h"
#include <QObject>

namespace RuleUtils {
QString normalizeRuleTypeKey(const QString &type)
{
    const QString trimmed = type.trimmed();
    if (trimmed.isEmpty()) return "default";
    return trimmed.toLower();
}

QString displayRuleTypeLabel(const QString &type)
{
    const QString trimmed = type.trimmed();
    if (trimmed.isEmpty()) return QObject::tr("Default");
    return type;
}

QString normalizeProxyValue(const QString &proxy)
{
    QString value = proxy.trimmed();
    if (value.compare("direct", Qt::CaseInsensitive) == 0) return "direct";
    if (value.compare("reject", Qt::CaseInsensitive) == 0) return "reject";
    if (value.startsWith('[') && value.endsWith(']')) {
        value = value.mid(1, value.length() - 2);
    }
    if (value.startsWith("Proxy(") && value.endsWith(")")) {
        value = value.mid(6, value.length() - 7);
    }
    if (value.startsWith("route(") && value.endsWith(")")) {
        value = value.mid(6, value.length() - 7);
    }
    return value;
}

QString displayProxyLabel(const QString &proxy)
{
    const QString value = normalizeProxyValue(proxy);
    if (value == "direct") return QObject::tr("Direct");
    if (value == "reject") return QObject::tr("Reject");
    return value;
}

bool isCustomPayload(const QString &payload)
{
    const QString p = payload.toLower();
    return p.startsWith("domain")
        || p.startsWith("ip")
        || p.startsWith("process")
        || p.startsWith("package")
        || p.startsWith("port")
        || p.startsWith("source");
}
} // namespace RuleUtils
