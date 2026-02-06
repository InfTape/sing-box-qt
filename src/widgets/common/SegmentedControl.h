#pragma once
#include <QPropertyAnimation>
#include <QStringList>
#include <QWidget>
class ThemeService;

class SegmentedControl : public QWidget {
  Q_OBJECT
  Q_PROPERTY(qreal selectionOffset READ selectionOffset WRITE setSelectionOffset)
 public:
  explicit SegmentedControl(QWidget* parent = nullptr, ThemeService* themeService = nullptr);
  void setItems(const QStringList& labels, const QStringList& values);
  void setCurrentIndex(int index);

  int currentIndex() const { return m_currentIndex; }

  QString currentValue() const;
  void    setThemeService(ThemeService* themeService);

  qreal selectionOffset() const { return m_selectionOffset; }

  void  setSelectionOffset(qreal offset);
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;
 signals:
  void currentIndexChanged(int index);
  void currentValueChanged(const QString& value);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

 private:
  int    indexAtPos(const QPoint& pos) const;
  QRectF itemRect(int index) const;
  QRectF selectionRect() const;
  void   animateToIndex(int index);
  void   recalculateLayout() const;

 private:
  QStringList            m_labels;
  QStringList            m_values;
  int                    m_currentIndex    = 0;
  int                    m_pressedIndex    = -1;
  qreal                  m_selectionOffset = 0.0;
  mutable QVector<qreal> m_itemWidths;
  mutable qreal          m_totalWidth   = 0;
  QPropertyAnimation*    m_anim         = nullptr;
  ThemeService*          m_themeService = nullptr;
};
