#pragma once

#include <QString>
#include <QStringList>

class ProxyService;
class DelayTestService;
struct DelayTestOptions;

namespace ProxyActions {

bool selectProxy(ProxyService *service, const QString &group, const QString &node);
bool startNodeDelayTest(DelayTestService *service, const QString &node, const DelayTestOptions &options);
bool startNodesDelayTests(DelayTestService *service, const QStringList &nodes, const DelayTestOptions &options);
void stopAllTests(DelayTestService *service);

} // namespace ProxyActions
