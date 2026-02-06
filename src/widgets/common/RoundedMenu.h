#ifndef ROUNDEDMENU_H
#define ROUNDEDMENU_H
#include <QColor>
#include <QMenu>

class RoundedMenu : public QMenu {
  Q_OBJECT
 public:
  explicit RoundedMenu(QWidget* parent = nullptr);
  void setThemeColors(const QColor& bg, const QColor& border);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  QColor m_bgColor;
  QColor m_borderColor;
};
#endif  // ROUNDEDMENU_H
