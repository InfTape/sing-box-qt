#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QObject>
#include <QString>

/**
 * @brief 应用设置管理类
 * 
 * 管理代理端口、TUN、DNS、功能开关等配置，
 * 仿照 Tauri 的 AppConfig 结构设计。
 */
class AppSettings : public QObject
{
    Q_OBJECT

public:
    static AppSettings& instance();
    
    // ==================== 端口配置 ====================
    int mixedPort() const { return m_mixedPort; }
    int apiPort() const { return m_apiPort; }
    void setMixedPort(int port);
    void setApiPort(int port);
    
    // ==================== TUN 配置 ====================
    bool tunEnabled() const { return m_tunEnabled; }
    bool tunAutoRoute() const { return m_tunAutoRoute; }
    bool tunStrictRoute() const { return m_tunStrictRoute; }
    QString tunStack() const { return m_tunStack; }
    int tunMtu() const { return m_tunMtu; }
    QString tunIpv4() const { return m_tunIpv4; }
    QString tunIpv6() const { return m_tunIpv6; }
    bool tunEnableIpv6() const { return m_tunEnableIpv6; }
    
    void setTunEnabled(bool enabled);
    void setTunAutoRoute(bool enabled);
    void setTunStrictRoute(bool enabled);
    void setTunStack(const QString &stack);
    void setTunMtu(int mtu);
    void setTunIpv4(const QString &addr);
    void setTunIpv6(const QString &addr);
    void setTunEnableIpv6(bool enabled);
    
    // ==================== DNS 配置 ====================
    QString dnsProxy() const { return m_dnsProxy; }
    QString dnsCn() const { return m_dnsCn; }
    QString dnsResolver() const { return m_dnsResolver; }
    
    void setDnsProxy(const QString &dns);
    void setDnsCn(const QString &dns);
    void setDnsResolver(const QString &dns);
    
    // ==================== 功能开关 ====================
    bool blockAds() const { return m_blockAds; }
    bool enableAppGroups() const { return m_enableAppGroups; }
    bool preferIpv6() const { return m_preferIpv6; }
    bool dnsHijack() const { return m_dnsHijack; }
    bool systemProxyEnabled() const { return m_systemProxyEnabled; }
    
    void setBlockAds(bool enabled);
    void setEnableAppGroups(bool enabled);
    void setPreferIpv6(bool enabled);
    void setDnsHijack(bool enabled);
    void setSystemProxyEnabled(bool enabled);
    
    // ==================== URL 测试配置 ====================
    QString urltestUrl() const { return m_urltestUrl; }
    void setUrltestUrl(const QString &url);
    
    // ==================== 默认出站选择 ====================
    // "auto" 表示默认使用自动选择组，"manual" 表示使用手动切换组
    QString defaultOutbound() const { return m_defaultOutbound; }
    // 规则集下载使用的出站："direct" 或 "manual"
    QString downloadDetour() const { return m_downloadDetour; }
    
    void setDefaultOutbound(const QString &outbound);
    void setDownloadDetour(const QString &detour);
    
    // ==================== 辅助方法 ====================
    // 获取规范化的默认出站标签
    QString normalizedDefaultOutbound() const;
    // 获取规范化的下载出站标签
    QString normalizedDownloadDetour() const;
    // 获取 DNS 策略字符串
    QString dnsStrategy() const;
    
    // 加载和保存
    void load();
    void save();

signals:
    void settingsChanged();

private:
    explicit AppSettings(QObject *parent = nullptr);
    ~AppSettings() = default;
    
    // 端口
    int m_mixedPort;
    int m_apiPort;
    
    // TUN
    bool m_tunEnabled;
    bool m_tunAutoRoute;
    bool m_tunStrictRoute;
    QString m_tunStack;
    int m_tunMtu;
    QString m_tunIpv4;
    QString m_tunIpv6;
    bool m_tunEnableIpv6;
    
    // DNS
    QString m_dnsProxy;
    QString m_dnsCn;
    QString m_dnsResolver;
    
    // 功能开关
    bool m_blockAds;
    bool m_enableAppGroups;
    bool m_preferIpv6;
    bool m_dnsHijack;
    bool m_systemProxyEnabled;
    
    // URL 测试
    QString m_urltestUrl;
    
    // 出站选择
    QString m_defaultOutbound;
    QString m_downloadDetour;
};

#endif // APPSETTINGS_H
