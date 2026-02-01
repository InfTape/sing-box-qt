#ifndef CONFIGREPOSITORY_H
#define CONFIGREPOSITORY_H

#include <QString>
#include <QJsonArray>

class ConfigRepository
{
public:
    virtual ~ConfigRepository() = default;

    virtual QString getActiveConfigPath() const = 0;
    virtual bool generateConfigWithNodes(const QJsonArray &nodes, const QString &targetPath = QString()) = 0;
    virtual bool updateClashDefaultMode(const QString &configPath, const QString &mode, QString *error = nullptr) = 0;
    virtual QString readClashDefaultMode(const QString &configPath) const = 0;
};

#endif // CONFIGREPOSITORY_H
