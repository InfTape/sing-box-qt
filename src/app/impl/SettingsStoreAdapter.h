#ifndef SETTINGSSTOREADAPTER_H
#define SETTINGSSTOREADAPTER_H
#include "app/interfaces/SettingsStore.h"
class SettingsStoreAdapter : public SettingsStore {
 public:
  bool systemProxyEnabled() const override;
  void setSystemProxyEnabled(bool enabled) override;
  bool tunEnabled() const override;
  void setTunEnabled(bool enabled) override;
};
#endif  // SETTINGSSTOREADAPTER_H
