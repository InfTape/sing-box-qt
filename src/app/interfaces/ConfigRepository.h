#ifndef CONFIGREPOSITORY_H
#define CONFIGREPOSITORY_H
#include <QJsonArray>
#include <QString>

class ConfigRepository {
 public:
  virtual ~ConfigRepository()                                                                             = default;
  virtual QString getConfigDir() const                                                                    = 0;
  virtual QString getActiveConfigPath() const                                                             = 0;
  virtual bool    generateConfigWithNodes(const QJsonArray& nodes, const QString& targetPath = QString()) = 0;
  virtual bool    updateClashDefaultMode(const QString& configPath, const QString& mode, QString* error = nullptr) = 0;
  virtual QString readClashDefaultMode(const QString& configPath) const                                            = 0;
  virtual QJsonObject loadConfig(const QString& path)                                                              = 0;
  virtual bool        saveConfig(const QString& path, const QJsonObject& config)                                   = 0;
  virtual void        applySettingsToConfig(QJsonObject& config)                                                   = 0;
  virtual void        applyPortSettings(QJsonObject& config)                                                       = 0;
  virtual int         mixedPort() const                                                                            = 0;
};
#endif  // CONFIGREPOSITORY_H
