#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    static ConfigManager &instance();

    QString getConfigDir() const;
    QString getActiveConfigPath() const;

    QJsonObject generateBaseConfig();
    bool generateConfigWithNodes(const QJsonArray &nodes, const QString &targetPath = QString());
    bool injectNodes(QJsonObject &config, const QJsonArray &nodes);

    void applySettingsToConfig(QJsonObject &config);
    void applyPortSettings(QJsonObject &config);

    QJsonObject loadConfig(const QString &path);
    bool saveConfig(const QString &path, const QJsonObject &config);

    int getMixedPort() const;
    int getApiPort() const;
    void setMixedPort(int port);
    void setApiPort(int port);

    bool updateClashDefaultMode(const QString &configPath, const QString &mode, QString *error = nullptr);
    QString readClashDefaultMode(const QString &configPath) const;

private:
    explicit ConfigManager(QObject *parent = nullptr);
};

#endif // CONFIGMANAGER_H
