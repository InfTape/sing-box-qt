#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

/**
 * @brief Sing-box 配置管理器
 * 
 * 负责生成、加载和保存 sing-box 配置文件。
 * 仿照 Tauri 项目的 config_generator.rs 和 settings_patch.rs 实现。
 */
class ConfigManager : public QObject
{
    Q_OBJECT

public:
    static ConfigManager& instance();
    
    // ==================== 配置文件路径 ====================
    QString getConfigDir() const;
    QString getActiveConfigPath() const;
    
    // ==================== 配置生成 ====================
    
    /**
     * @brief 生成基础配置骨架（不含节点）
     * 
     * 包含完整的 DNS、路由规则、入站、出站组配置，
     * 但 urltest 和 selector 的候选列表为空。
     */
    QJsonObject generateBaseConfig();
    
    /**
     * @brief 生成带节点的完整配置
     * 
     * 基于 generateBaseConfig() 生成骨架，
     * 然后调用 injectNodes() 注入节点。
     * 
     * @param nodes 节点数组
     * @param targetPath 目标路径，空则使用默认路径
     * @return 是否成功
     */
    bool generateConfigWithNodes(const QJsonArray &nodes,
                                 const QString &targetPath = QString());
    
    /**
     * @brief 注入节点到配置
     * 
     * - 处理节点 tag 冲突
     * - 自动为域名节点添加 domain_resolver
     * - 更新 urltest/selector 组的候选列表
     * - 更新分流组的候选列表
     */
    bool injectNodes(QJsonObject &config, const QJsonArray &nodes);
    
    /**
     * @brief 应用设置到配置（端口、TUN、DNS 等）
     */
    void applySettingsToConfig(QJsonObject &config);
    
    /**
     * @brief 仅应用端口设置
     */
    void applyPortSettings(QJsonObject &config);
    
    // ==================== 文件操作 ====================
    QJsonObject loadConfig(const QString &path);
    bool saveConfig(const QString &path, const QJsonObject &config);
    
    // ==================== 端口访问（保留兼容） ====================
    int getMixedPort() const;
    int getApiPort() const;
    void setMixedPort(int port);
    void setApiPort(int port);
    bool updateClashDefaultMode(const QString &configPath, const QString &mode, QString *error = nullptr);
    QString readClashDefaultMode(const QString &configPath) const;

private:
    explicit ConfigManager(QObject *parent = nullptr);
    
    // ==================== 配置构建器 ====================
    
    // 构建 DNS 配置
    QJsonObject buildDnsConfig();
    
    // 构建路由配置
    QJsonObject buildRouteConfig();
    
    // 构建入站配置
    QJsonArray buildInbounds();
    
    // 构建出站选择器组
    QJsonArray buildOutboundGroups();
    
    // 构建规则集列表
    QJsonArray buildRuleSets();
    
    // 构建实验性功能配置
    QJsonObject buildExperimental();
    
    // ==================== 节点注入辅助 ====================
    
    // 判断节点是否应加入分组
    bool shouldIncludeNodeInGroups(const QJsonObject &node);
    
    // 确保出站组存在并返回索引
    int ensureOutboundIndex(QJsonArray &outbounds, const QString &tag);
    
    // 更新 urltest 和 selector 组
    void updateUrltestAndSelector(QJsonArray &outbounds, const QStringList &nodeTags);
    
    // 更新分流组（Telegram/YouTube/Netflix/OpenAI）
    void updateAppGroupSelectors(QJsonArray &outbounds, const QStringList &nodeTags);
    
    // ==================== 成员变量（保留兼容） ====================
    int m_mixedPort;
    int m_apiPort;
};

#endif // CONFIGMANAGER_H
