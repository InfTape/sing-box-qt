#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QObject>
#include <QString>

/**
 * @brief Application settings manager.
 *
 * Manages proxy ports, TUN, DNS, and feature flags.
 * Inspired by Tauri's AppConfig structure.
 */
class AppSettings : public QObject
{
    Q_OBJECT

public:
    static AppSettings& instance();

    // Port config.
    int mixedPort() const { return m_mixedPort; }
    int apiPort() const { return m_apiPort; }
    void setMixedPort(int port);
    void setApiPort(int port);

    // TUN config.
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

    // DNS config.
    QString dnsProxy() const { return m_dnsProxy; }
    QString dnsCn() const { return m_dnsCn; }
    QString dnsResolver() const { return m_dnsResolver; }

    void setDnsProxy(const QString &dns);
    void setDnsCn(const QString &dns);
    void setDnsResolver(const QString &dns);

    // Feature flags.
    bool blockAds() const { return m_blockAds; }
    bool enableAppGroups() const { return m_enableAppGroups; }
    bool preferIpv6() const { return m_preferIpv6; }
    bool dnsHijack() const { return m_dnsHijack; }
    bool systemProxyEnabled() const { return m_systemProxyEnabled; }
    QString systemProxyBypass() const { return m_systemProxyBypass; }

    void setBlockAds(bool enabled);
    void setEnableAppGroups(bool enabled);
    void setPreferIpv6(bool enabled);
    void setDnsHijack(bool enabled);
    void setSystemProxyEnabled(bool enabled);
    void setSystemProxyBypass(const QString &bypass);

    // URL test config.
    QString urltestUrl() const { return m_urltestUrl; }
    int urltestTimeoutMs() const { return m_urltestTimeoutMs; }
    int urltestConcurrency() const { return m_urltestConcurrency; }
    int urltestSamples() const { return m_urltestSamples; }
    void setUrltestTimeoutMs(int ms);
    void setUrltestConcurrency(int c);
    void setUrltestSamples(int s);
    void setUrltestUrl(const QString &url);

    // Default outbound selection.
    // "auto" uses the auto-select group, "manual" uses the manual group.
    QString defaultOutbound() const { return m_defaultOutbound; }
    // Detour for rule set downloads: "direct" or "manual".
    QString downloadDetour() const { return m_downloadDetour; }

    void setDefaultOutbound(const QString &outbound);
    void setDownloadDetour(const QString &detour);

    // Helper methods.
    QString normalizedDefaultOutbound() const;
    QString normalizedDownloadDetour() const;
    QString dnsStrategy() const;

    // Load and save.
    void load();
    void save();

signals:
    void settingsChanged();

private:
    explicit AppSettings(QObject *parent = nullptr);
    ~AppSettings() = default;

    // Ports.
    int m_mixedPort;
    int m_apiPort;

    // TUN.
    bool m_tunEnabled;
    bool m_tunAutoRoute;
    bool m_tunStrictRoute;
    QString m_tunStack;
    int m_tunMtu;
    QString m_tunIpv4;
    QString m_tunIpv6;
    bool m_tunEnableIpv6;

    // DNS.
    QString m_dnsProxy;
    QString m_dnsCn;
    QString m_dnsResolver;

    // Feature flags.
    bool m_blockAds;
    bool m_enableAppGroups;
    bool m_preferIpv6;
    bool m_dnsHijack;
    bool m_systemProxyEnabled;
    QString m_systemProxyBypass;

    // URL test.
    QString m_urltestUrl;
    int m_urltestTimeoutMs;
    int m_urltestConcurrency;
    int m_urltestSamples;

    // Outbound selection.
    QString m_defaultOutbound;
    QString m_downloadDetour;
};

#endif // APPSETTINGS_H
