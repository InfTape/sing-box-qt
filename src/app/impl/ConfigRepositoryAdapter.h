#ifndef CONFIGREPOSITORYADAPTER_H
#define CONFIGREPOSITORYADAPTER_H

#include "app/interfaces/ConfigRepository.h"

class ConfigRepositoryAdapter : public ConfigRepository
{
public:
    QString getActiveConfigPath() const override;
    bool generateConfigWithNodes(const QJsonArray &nodes, const QString &targetPath = QString()) override;
    bool updateClashDefaultMode(const QString &configPath, const QString &mode, QString *error = nullptr) override;
    QString readClashDefaultMode(const QString &configPath) const override;
};

#endif // CONFIGREPOSITORYADAPTER_H
