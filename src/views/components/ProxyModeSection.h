#ifndef PROXYMODESECTION_H
#define PROXYMODESECTION_H
#include <QWidget>
class QLabel;
class ToggleSwitch;
class ThemeService;

class ProxyModeSection : public QWidget {
  Q_OBJECT
 public:
  explicit ProxyModeSection(ThemeService* themeService = nullptr,
                            QWidget*      parent       = nullptr);
  bool isSystemProxyEnabled() const;
  void setSystemProxyEnabled(bool enabled);
  bool isTunModeEnabled() const;
  void setTunModeEnabled(bool enabled);
  void setProxyMode(const QString& mode);
  void updateStyle();
 signals:
  void systemProxyChanged(bool enabled);
  void tunModeChanged(bool enabled);
  void proxyModeChanged(const QString& mode);

 private:
  void     setupUi();
  QWidget* createModeItem(const QString& iconText,
                          const QString& accentKey,
                          const QString& title,
                          const QString& desc,
                          QWidget*       control);
  void     setCardActive(QWidget* card, bool active);

 private:
  ThemeService* m_themeService      = nullptr;
  QWidget*      m_systemProxyCard   = nullptr;
  QWidget*      m_tunModeCard       = nullptr;
  QWidget*      m_globalModeCard    = nullptr;
  QWidget*      m_ruleModeCard      = nullptr;
  ToggleSwitch* m_systemProxySwitch = nullptr;
  ToggleSwitch* m_tunModeSwitch     = nullptr;
  ToggleSwitch* m_globalModeSwitch  = nullptr;
  ToggleSwitch* m_ruleModeSwitch    = nullptr;
};
#endif  // PROXYMODESECTION_H
