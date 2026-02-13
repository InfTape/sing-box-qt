#ifndef GRIDCOMBOBOX_H
#define GRIDCOMBOBOX_H
#include <QComboBox>
class QListWidget;
class QPaintEvent;
class QWheelEvent;
class ThemeService;
class RoundedMenu;

class GridComboBox : public QComboBox {
  Q_OBJECT
 public:
  explicit GridComboBox(QWidget*      parent       = nullptr,
                        ThemeService* themeService = nullptr);

  void setWheelEnabled(bool enabled);
  bool isWheelEnabled() const { return m_wheelEnabled; }

  void setMaxColumns(int columns);
  void setVisibleRowRange(int minRows, int maxRows);
  void setThemeService(ThemeService* themeService);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void showPopup() override;
  void hidePopup() override;
  void wheelEvent(QWheelEvent* event) override;

 private:
  void updatePopupLayout();
  void updateMenuStyle();

  RoundedMenu* m_menu         = nullptr;
  QWidget*     m_popupContent = nullptr;
  QListWidget* m_listWidget   = nullptr;
  bool         m_wheelEnabled = true;
  int          m_maxColumns   = 3;
  int          m_minVisibleRows = 4;
  int          m_maxVisibleRows = 9;
  ThemeService* m_themeService  = nullptr;
};

#endif  // GRIDCOMBOBOX_H
