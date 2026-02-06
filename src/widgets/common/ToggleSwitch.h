#pragma once
#include <QPropertyAnimation>
#include <QWidget>
class ThemeService;

class ToggleSwitch : public QWidget {
  Q_OBJECT
  Q_PROPERTY(qreal offset READ offset WRITE setOffset)
 public:
  explicit ToggleSwitch(QWidget* parent = nullptr, ThemeService* themeService = nullptr);

  bool isChecked() const { return m_checked; }

  void setChecked(bool checked);

  void setThemeService(ThemeService* themeService) { m_themeService = themeService; }

  qreal offset() const { return m_offset; }

  void setOffset(qreal v);

  QSize sizeHint() const override { return {52, 30}; }

 signals:
  void toggled(bool checked);

 protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void keyPressEvent(QKeyEvent*) override;

 private:
  QRectF trackRect() const;
  QRectF thumbRectForOffset(qreal off) const;
  void   animateTo(bool checked);
  void   setCheckedInternal(bool checked, bool emitSignal);

 private:
  bool                m_checked  = false;
  qreal               m_offset   = 0.0;  // 0 = left(off), 1 = right(on)
  bool                m_dragging = false;
  QPoint              m_pressPos;
  qreal               m_pressOffset  = 0.0;
  QPropertyAnimation* m_anim         = nullptr;
  ThemeService*       m_themeService = nullptr;
};
