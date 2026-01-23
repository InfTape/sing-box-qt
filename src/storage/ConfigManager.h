#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

/**
 * @brief Sing-box config manager.
 *
 * Generates, loads, and saves sing-box config files.
 * Inspired by Tauri's config_generator.rs and settings_patch.rs.
 */
class ConfigManager : public QObject
{
    Q_OBJECT

public:
    static ConfigManager& instance();

    // Config paths.
    QString getConfigDir() const;
    QString getActiveConfigPath() const;

    // Config generation.

    /**
     * @brief Generate base config skeleton without nodes.
     *
     * Includes DNS, route rules, inbounds, and outbound groups.
     * The urltest and selector candidate lists are empty.
     */
    QJsonObject generateBaseConfig();

    /**
     * @brief Generate full config with nodes.
     *
     * Builds the base skeleton and injects nodes.
     *
     * @param nodes Node array.
     * @param targetPath Output path; uses default when empty.
     * @return True on success.
     */
    bool generateConfigWithNodes(const QJsonArray &nodes,
                                 const QString &targetPath = QString());

    /**
     * @brief Inject nodes into config.
     *
     * - Resolve tag conflicts
     * - Add domain_resolver for domain nodes
     * - Update urltest/selector candidates
     * - Update app group candidates
     */
    bool injectNodes(QJsonObject &config, const QJsonArray &nodes);

    /**
     * @brief Apply settings to config (ports, TUN, DNS, etc).
     */
    void applySettingsToConfig(QJsonObject &config);

    /**
     * @brief Apply only port settings.
     */
    void applyPortSettings(QJsonObject &config);

    // File operations.
    QJsonObject loadConfig(const QString &path);
    bool saveConfig(const QString &path, const QJsonObject &config);

    // Port accessors (compat).
    int getMixedPort() const;
    int getApiPort() const;
    void setMixedPort(int port);
    void setApiPort(int port);
    bool updateClashDefaultMode(const QString &configPath, const QString &mode, QString *error = nullptr);
    QString readClashDefaultMode(const QString &configPath) const;

private:
    explicit ConfigManager(QObject *parent = nullptr);

    // Config builders.
    QJsonObject buildDnsConfig();
    QJsonObject buildRouteConfig();
    QJsonArray buildInbounds();
    QJsonArray buildOutboundGroups();
    QJsonArray buildRuleSets();
    QJsonObject buildExperimental();

    // Node injection helpers.
    bool shouldIncludeNodeInGroups(const QJsonObject &node);
    int ensureOutboundIndex(QJsonArray &outbounds, const QString &tag);
    void updateUrltestAndSelector(QJsonArray &outbounds, const QStringList &nodeTags);
    void updateAppGroupSelectors(QJsonArray &outbounds, const QStringList &nodeTags);

    // Member variables (compat).
    int m_mixedPort;
    int m_apiPort;
};

#endif // CONFIGMANAGER_H
