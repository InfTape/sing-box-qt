#include "SettingsStoreAdapter.h"
#include "storage/AppSettings.h"
bool SettingsStoreAdapter::systemProxyEnabled() const {
  return AppSettings::instance().systemProxyEnabled();
}
void SettingsStoreAdapter::setSystemProxyEnabled(bool enabled) {
  AppSettings::instance().setSystemProxyEnabled(enabled);
}
bool SettingsStoreAdapter::tunEnabled() const {
  return AppSettings::instance().tunEnabled();
}
void SettingsStoreAdapter::setTunEnabled(bool enabled) {
  AppSettings::instance().setTunEnabled(enabled);
}
