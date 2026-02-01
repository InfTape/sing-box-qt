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
  // Write/Remove shared rules to/from current config (entries with shared=true in route.rules)
  static void applySharedRules(QJsonObject&      config,
                               const QJsonArray& sharedRules, bool enabled);
};
#endif  // CONFIGMUTATOR_H
