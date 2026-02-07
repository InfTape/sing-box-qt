#ifndef SETTINGSMODEL_H
#define SETTINGSMODEL_H
#include <QString>

class SettingsModel {
 public:
  struct Data {
    int     mixedPort          = 7890;
    int     apiPort            = 9090;
    bool    autoStart          = false;
    bool    systemProxyEnabled = true;
    QString systemProxyBypass;
    int     tunMtu          = 1500;
    QString tunStack        = "mixed";
    bool    tunEnableIpv6   = false;
    bool    tunAutoRoute    = true;
    bool    tunStrictRoute  = true;
    QString defaultOutbound = "manual";
    QString downloadDetour  = "manual";
    bool    blockAds        = false;
    bool    dnsHijack       = true;
    bool    enableAppGroups = false;
    QString dnsProxy;
    QString dnsCn;
    QString dnsResolver;
    QString urltestUrl;
  };

  static Data load();
  static bool save(const Data& data, QString* errorMessage = nullptr);
};
#endif  // SETTINGSMODEL_H
