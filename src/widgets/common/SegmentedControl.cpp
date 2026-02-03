#include "widgets/common/SegmentedControl.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

#include "app/interfaces/ThemeService.h"

SegmentedControl::SegmentedControl(QWidget* parent, ThemeService* themeService)
    : QWidget(parent), m_themeService(themeService) {
  setCursor(Qt::PointingHandCursor);
  setFocusPolicy(Qt::StrongFocus);

  m_anim = new QPropertyAnimation(this, "selectionOffset", this);
  m_anim->setDuration(150);
  m_anim->setEasingCurve(QEasingCurve::OutCubic);

  if (m_themeService) {
    connect(m_themeService, &ThemeService::themeChanged, this,
            qOverload<>(&SegmentedControl::update));
  }
}

void SegmentedControl::setItems(const QStringList& labels,
                                const QStringList& values) {
  m_labels = labels;
  m_values = values;
  m_currentIndex = 0;
  m_selectionOffset = 0.0;
  recalculateLayout();
  update();
}

void SegmentedControl::setCurrentIndex(int index) {
  if (index < 0 || index >= m_labels.size()) return;
  if (index == m_currentIndex) return;
  animateToIndex(index);
  m_currentIndex = index;
  emit currentIndexChanged(index);
  emit currentValueChanged(currentValue());
}

QString SegmentedControl::currentValue() const {
  if (m_currentIndex >= 0 && m_currentIndex < m_values.size()) {
    return m_values.at(m_currentIndex);
  }
  return QString();
}

void SegmentedControl::setThemeService(ThemeService* themeService) {
  m_themeService = themeService;
  if (m_themeService) {
    connect(m_themeService, &ThemeService::themeChanged, this,
            qOverload<>(&SegmentedControl::update));
  }
  update();
}

void SegmentedControl::setSelectionOffset(qreal offset) {
  if (qFuzzyCompare(m_selectionOffset, offset)) return;
  m_selectionOffset = offset;
  update();
}

QSize SegmentedControl::sizeHint() const {
  recalculateLayout();
  int h = fontMetrics().height() + 12;
  return QSize(static_cast<int>(m_totalWidth) + 8, h);
}

QSize SegmentedControl::minimumSizeHint() const {
  return sizeHint();
}

void SegmentedControl::recalculateLayout() const {
  QFontMetrics fm = fontMetrics();
  m_itemWidths.clear();
  m_totalWidth = 0;

  for (const QString& label : m_labels) {
    qreal w = fm.horizontalAdvance(label) + 24; // padding
    m_itemWidths.append(w);
    m_totalWidth += w;
  }
}

int SegmentedControl::indexAtPos(const QPoint& pos) const {
  if (m_labels.isEmpty()) return -1;

  qreal x = 4; // left margin
  for (int i = 0; i < m_itemWidths.size(); ++i) {
    if (pos.x() >= x && pos.x() < x + m_itemWidths[i]) {
      return i;
    }
    x += m_itemWidths[i];
  }
  return -1;
}

QRectF SegmentedControl::itemRect(int index) const {
  if (index < 0 || index >= m_itemWidths.size()) return QRectF();

  qreal x = 4; // left margin
  for (int i = 0; i < index; ++i) {
    x += m_itemWidths[i];
  }

  qreal h = height() - 4;
  return QRectF(x, 2, m_itemWidths[index], h);
}

QRectF SegmentedControl::selectionRect() const {
  if (m_labels.isEmpty()) return QRectF();

  // Interpolate between current position based on offset
  int prevIndex = static_cast<int>(m_selectionOffset);
  int nextIndex = qMin(prevIndex + 1, m_labels.size() - 1);
  qreal frac = m_selectionOffset - prevIndex;

  QRectF prevRect = itemRect(prevIndex);
  QRectF nextRect = itemRect(nextIndex);

  qreal x = prevRect.x() + (nextRect.x() - prevRect.x()) * frac;
  qreal w = prevRect.width() + (nextRect.width() - prevRect.width()) * frac;

  return QRectF(x, prevRect.y(), w, prevRect.height());
}

void SegmentedControl::animateToIndex(int index) {
  m_anim->stop();
  m_anim->setStartValue(m_selectionOffset);
  m_anim->setEndValue(static_cast<qreal>(index));
  m_anim->start();
}

void SegmentedControl::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  ThemeService* ts = m_themeService;

  // Background colors
  QColor bgColor = ts ? ts->color("bg-secondary") : QColor(240, 240, 240);
  QColor selColor = ts ? ts->color("bg-tertiary") : QColor(60, 60, 70);
  QColor textColor = ts ? ts->color("text-secondary") : QColor(100, 100, 100);
  QColor selectedTextColor = ts ? ts->color("text-primary") : QColor(0, 0, 0);
  QColor borderColor = ts ? ts->color("border") : QColor(200, 200, 200);

  // Draw background
  QRectF bgRect = rect().adjusted(1, 1, -1, -1);
  qreal radius = bgRect.height() / 2.0;
  p.setPen(Qt::NoPen);
  p.setBrush(bgColor);
  p.drawRoundedRect(bgRect, radius, radius);

  // Draw selection indicator
  if (!m_labels.isEmpty()) {
    QRectF sel = selectionRect();
    sel.adjust(2, 2, -2, -2);
    qreal selRadius = sel.height() / 2.0;

    // Shadow for selection
    QColor shadowColor(0, 0, 0, 20);
    QRectF shadowRect = sel.translated(0, 1);
    p.setBrush(shadowColor);
    p.drawRoundedRect(shadowRect, selRadius, selRadius);

    // Selection background
    p.setBrush(selColor);
    p.setPen(QPen(borderColor, 0.5));
    p.drawRoundedRect(sel, selRadius, selRadius);
  }

  // Draw labels
  QFont font = this->font();
  p.setFont(font);

  for (int i = 0; i < m_labels.size(); ++i) {
    QRectF r = itemRect(i);

    // Determine text color based on selection
    qreal dist = qAbs(m_selectionOffset - i);
    QColor color;
    if (dist < 0.5) {
      // Interpolate to selected color
      qreal t = 1.0 - dist * 2.0;
      color = QColor(
          static_cast<int>(textColor.red() + (selectedTextColor.red() - textColor.red()) * t),
          static_cast<int>(textColor.green() + (selectedTextColor.green() - textColor.green()) * t),
          static_cast<int>(textColor.blue() + (selectedTextColor.blue() - textColor.blue()) * t));
    } else {
      color = textColor;
    }

    p.setPen(color);
    p.drawText(r, Qt::AlignCenter, m_labels.at(i));
  }
}

void SegmentedControl::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  m_pressedIndex = indexAtPos(event->pos());
  event->accept();
}

void SegmentedControl::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;

  int releasedIndex = indexAtPos(event->pos());
  if (releasedIndex >= 0 && releasedIndex == m_pressedIndex) {
    setCurrentIndex(releasedIndex);
  }

  m_pressedIndex = -1;
  event->accept();
}

void SegmentedControl::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  recalculateLayout();
}
