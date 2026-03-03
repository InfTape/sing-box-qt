#pragma once

#include <QObject>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include "network/SubscriptionService.h"
#include "services/config/ConfigBuilder.h"
#include "services/config/ConfigMutator.h"
#include "services/rules/RuleConfigService.h"
#include "services/subscription/SubscriptionParser.h"
#include "services/kernel/KernelPlatform.h"
#include "storage/AppSettings.h"
#include "storage/ConfigConstants.h"
#include "storage/DatabaseService.h"
#include "utils/Crypto.h"
#include "utils/GitHubMirror.h"
#include "utils/LogParser.h"
#include "utils/home/HomeFormat.h"
#include "core/DataUsageTracker.h"
#include "utils/proxy/ProxyNodeHelper.h"
#include "utils/rule/RuleUtils.h"
#include "utils/settings/SettingsHelpers.h"
#include "utils/subscription/SubscriptionHelpers.h"
#include "utils/subscription/SubscriptionFormat.h"
#include "utils/subscription/SubscriptionUserinfo.h"

namespace {
QJsonObject findObjectByTag(const QJsonArray& arr, const QString& tag) {
  for (const auto& v : arr) {
    if (!v.isObject()) {
      continue;
    }
    const QJsonObject obj = v.toObject();
    if (obj.value("tag").toString() == tag) {
      return obj;
    }
  }
  return QJsonObject();
}

int findRuleSetIndex(const QJsonArray& rules, const QString& ruleSet) {
  for (int i = 0; i < rules.size(); ++i) {
    if (!rules[i].isObject()) {
      continue;
    }
    if (rules[i].toObject().value("rule_set").toString() == ruleSet) {
      return i;
    }
  }
  return -1;
}

int findTagIndex(const QJsonArray& arr, const QString& tag) {
  for (int i = 0; i < arr.size(); ++i) {
    if (!arr[i].isObject()) {
      continue;
    }
    if (arr[i].toObject().value("tag").toString() == tag) {
      return i;
    }
  }
  return -1;
}

int findProtocolActionIndex(const QJsonArray& rules,
                            const QString&    protocol,
                            const QString&    action) {
  for (int i = 0; i < rules.size(); ++i) {
    if (!rules[i].isObject()) {
      continue;
    }
    const QJsonObject obj = rules[i].toObject();
    if (obj.value("protocol").toString() == protocol &&
        obj.value("action").toString() == action) {
      return i;
    }
  }
  return -1;
}

int findActionIndex(const QJsonArray& rules, const QString& action) {
  for (int i = 0; i < rules.size(); ++i) {
    if (!rules[i].isObject()) {
      continue;
    }
    if (rules[i].toObject().value("action").toString() == action) {
      return i;
    }
  }
  return -1;
}

struct AppSettingsScopeGuard {
  AppSettings& s = AppSettings::instance();

  int     mixedPort;
  int     apiPort;
  bool    tunEnabled;
  bool    tunAutoRoute;
  bool    tunStrictRoute;
  QString tunStack;
  int     tunMtu;
  QString tunIpv4;
  QString tunIpv6;
  bool    tunEnableIpv6;
  bool    routeSniffEnabled;
  QString dnsProxy;
  QString dnsCn;
  QString dnsResolver;
  bool    blockAds;
  bool    enableAppGroups;
  bool    preferIpv6;
  bool    dnsHijack;
  bool    systemProxyEnabled;
  QString systemProxyBypass;
  QString urltestUrl;
  int     urltestTimeoutMs;
  int     urltestConcurrency;
  int     urltestSamples;
  QString defaultOutbound;
  QString downloadDetour;

  AppSettingsScopeGuard()
      : mixedPort(s.mixedPort()),
        apiPort(s.apiPort()),
        tunEnabled(s.tunEnabled()),
        tunAutoRoute(s.tunAutoRoute()),
        tunStrictRoute(s.tunStrictRoute()),
        tunStack(s.tunStack()),
        tunMtu(s.tunMtu()),
        tunIpv4(s.tunIpv4()),
        tunIpv6(s.tunIpv6()),
        tunEnableIpv6(s.tunEnableIpv6()),
        routeSniffEnabled(s.routeSniffEnabled()),
        dnsProxy(s.dnsProxy()),
        dnsCn(s.dnsCn()),
        dnsResolver(s.dnsResolver()),
        blockAds(s.blockAds()),
        enableAppGroups(s.enableAppGroups()),
        preferIpv6(s.preferIpv6()),
        dnsHijack(s.dnsHijack()),
        systemProxyEnabled(s.systemProxyEnabled()),
        systemProxyBypass(s.systemProxyBypass()),
        urltestUrl(s.urltestUrl()),
        urltestTimeoutMs(s.urltestTimeoutMs()),
        urltestConcurrency(s.urltestConcurrency()),
        urltestSamples(s.urltestSamples()),
        defaultOutbound(s.defaultOutbound()),
        downloadDetour(s.downloadDetour()) {}

  ~AppSettingsScopeGuard() {
    s.setMixedPort(mixedPort);
    s.setApiPort(apiPort);
    s.setTunEnabled(tunEnabled);
    s.setTunAutoRoute(tunAutoRoute);
    s.setTunStrictRoute(tunStrictRoute);
    s.setTunStack(tunStack);
    s.setTunMtu(tunMtu);
    s.setTunIpv4(tunIpv4);
    s.setTunIpv6(tunIpv6);
    s.setTunEnableIpv6(tunEnableIpv6);
    s.setRouteSniffEnabled(routeSniffEnabled);
    s.setDnsProxy(dnsProxy);
    s.setDnsCn(dnsCn);
    s.setDnsResolver(dnsResolver);
    s.setBlockAds(blockAds);
    s.setEnableAppGroups(enableAppGroups);
    s.setPreferIpv6(preferIpv6);
    s.setDnsHijack(dnsHijack);
    s.setSystemProxyEnabled(systemProxyEnabled);
    s.setSystemProxyBypass(systemProxyBypass);
    s.setUrltestUrl(urltestUrl);
    s.setUrltestTimeoutMs(urltestTimeoutMs);
    s.setUrltestConcurrency(urltestConcurrency);
    s.setUrltestSamples(urltestSamples);
    s.setDefaultOutbound(defaultOutbound);
    s.setDownloadDetour(downloadDetour);
  }
};
}  // namespace

