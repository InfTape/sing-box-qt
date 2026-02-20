#include "utils/LogParser.h"
#include <QRegularExpression>

namespace LogParser {
LogKind parseLogKind(const QString& message) {
  LogKind                         info;
  static const QRegularExpression kDnsPattern("\\bdns\\s*:");
  static const QRegularExpression kOutboundNode(
      R"(outbound/([^\[]+)\[([^\]]+)\])");
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
  static const QRegularExpression kConnHost(
      R"(connection (?:from|to) ([^\s]+))");
  const QRegularExpressionMatch match = kConnHost.match(message);
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

QString logTypeLabel(const QString& type) {
  if (type == "trace") {
    return "TRACE";
  }
  if (type == "debug") {
    return "DEBUG";
  }
  if (type == "info") {
    return "INFO";
  }
  if (type == "warning") {
    return "WARN";
  }
  if (type == "error") {
    return "ERROR";
  }
  if (type == "fatal") {
    return "FATAL";
  }
  if (type == "panic") {
    return "PANIC";
  }
  return "INFO";
}
}  // namespace LogParser
