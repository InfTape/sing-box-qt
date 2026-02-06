#ifndef MENUCOMBOBOX_H
#define MENUCOMBOBOX_H
#include <QComboBox>
class QWheelEvent;
class QPaintEvent;
class ThemeService;
class RoundedMenu;
class MenuComboBox : public QComboBox {
  Q_OBJECT
 public:
  explicit MenuComboBox(QWidget* parent = nullptr, ThemeService* themeService = nullptr);
  void setWheelEnabled(bool enabled);
  bool isWheelEnabled() const { return m_wheelEnabled; }
  void setThemeService(ThemeService* themeService);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void showPopup() override;
  void hidePopup() override;
  void wheelEvent(QWheelEvent* event) override;

 private:
  void          updateMenuStyle();
  RoundedMenu*  m_menu         = nullptr;
  bool          m_wheelEnabled = true;
  ThemeService* m_themeService = nullptr;
};
#endif  // MENUCOMBOBOX_H
