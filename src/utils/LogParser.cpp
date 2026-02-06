#include "utils/LogParser.h"

#include <QRegularExpression>
namespace LogParser {
QString stripAnsiSequences(const QString& text) {
  static const QRegularExpression kAnsiPattern("\x1B\\[[0-?]*[ -/]*[@-~]");
  QString                         cleaned = text;
  cleaned.remove(kAnsiPattern);
  return cleaned;
}
LogKind parseLogKind(const QString& message) {
  LogKind                         info;
  static const QRegularExpression kDnsPattern("\\bdns\\s*:");
  static const QRegularExpression kOutboundNode(R"(outbound/([^\[]+)\[([^\]]+)\])");
  if (message.contains(kDnsPattern)) {
    info.direction = "dns";
    info.isDns     = true;
    return info;
  }

  if (message.contains("inbound connection")) {
    info.direction = "inbound";
  } else if (message.contains("outbound connection")) {
    info.direction = "outbound";
  } else {
    return info;
  }

  static const QRegularExpression kConnHost(R"(connection (?:from|to) ([^\s]+))");
  const QRegularExpressionMatch   match = kConnHost.match(message);
  if (match.hasMatch()) {
    info.host = match.captured(1);
  }

  if (info.direction == "outbound") {
    const QRegularExpressionMatch nodeMatch = kOutboundNode.match(message);
    if (nodeMatch.hasMatch()) {
      info.protocol = nodeMatch.captured(1).trimmed();
      info.nodeName = nodeMatch.captured(2).trimmed();
    }
  }

  info.isConnection = true;
  return info;
}
QString detectLogType(const QString& message) {
  const QString                   upper = message.toUpper();
  static const QRegularExpression kPanicRe("\\bPANIC\\b");
  static const QRegularExpression kFatalRe("\\bFATAL\\b");
  static const QRegularExpression kErrorRe("\\bERROR\\b");
  static const QRegularExpression kWarnRe("\\bWARN\\b");
  static const QRegularExpression kWarningRe("\\bWARNING\\b");
  static const QRegularExpression kDebugRe("\\bDEBUG\\b");
  static const QRegularExpression kTraceRe("\\bTRACE\\b");
  static const QRegularExpression kInfoRe("\\bINFO\\b");

  if (upper.contains(kPanicRe)) return "panic";
  if (upper.contains(kFatalRe)) return "fatal";
  if (upper.contains(kErrorRe)) return "error";
  if (upper.contains(kWarnRe) || upper.contains(kWarningRe)) return "warning";
  if (upper.contains(kDebugRe)) return "debug";
  if (upper.contains(kTraceRe)) return "trace";
  if (upper.contains(kInfoRe)) return "info";
  return "info";
}
QString logTypeLabel(const QString& type) {
  if (type == "trace") return "TRACE";
  if (type == "debug") return "DEBUG";
  if (type == "info") return "INFO";
  if (type == "warning") return "WARN";
  if (type == "error") return "ERROR";
  if (type == "fatal") return "FATAL";
  if (type == "panic") return "PANIC";
  return "INFO";
}
}  // namespace LogParser
