#ifndef SETTINGSSTORE_H
#define SETTINGSSTORE_H
class QString;
class SettingsStore {
 public:
  virtual ~SettingsStore()                         = default;
  virtual bool systemProxyEnabled() const          = 0;
  virtual void setSystemProxyEnabled(bool enabled) = 0;
  virtual bool tunEnabled() const                  = 0;
  virtual void setTunEnabled(bool enabled)         = 0;
};
#endif  // SETTINGSSTORE_H
