#ifndef CONFIGMUTATOR_H
#define CONFIGMUTATOR_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
class ConfigMutator {
 public:
  static bool injectNodes(QJsonObject& config, const QJsonArray& nodes);
  static void applySettings(QJsonObject& config);
  static void applyPortSettings(QJsonObject& config);
  static bool updateClashDefaultMode(QJsonObject& config, const QString& mode,
                                     QString* error = nullptr);
  static QString readClashDefaultMode(const QJsonObject& config);
  // 将共享规则写入/移除当前配置（route.rules 中 shared=true 的条目）
  static void applySharedRules(QJsonObject&      config,
                               const QJsonArray& sharedRules, bool enabled);
};
#endif  // CONFIGMUTATOR_H
