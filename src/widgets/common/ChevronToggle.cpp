#include "widgets/common/ChevronToggle.h"

#include <QEasingCurve>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QStyleOption>
ChevronToggle::ChevronToggle(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_StyledBackground, true);
  setAttribute(Qt::WA_Hover, true);
  setCursor(Qt::PointingHandCursor);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}
void ChevronToggle::setExpanded(bool expanded) {
  if (m_expanded == expanded) return;
  m_expanded = expanded;
  if (!m_anim) {
    m_anim = new QPropertyAnimation(this, "progress", this);
    m_anim->setDuration(160);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
  }
  m_anim->stop();
  m_anim->setStartValue(m_progress);
  m_anim->setEndValue(m_expanded ? 1.0 : 0.0);
  m_anim->start();
  emit expandedChanged(m_expanded);
}
void ChevronToggle::setProgress(qreal value) {
  m_progress = qBound(0.0, value, 1.0);
  update();
}
void ChevronToggle::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    setExpanded(!m_expanded);
    emit clicked();
    emit toggled(m_expanded);
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}
void ChevronToggle::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event)

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  QStyleOption opt;
  opt.initFrom(this);
  style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);

  const qreal   w    = width();
  const qreal   h    = height();
  const qreal   size = qMin(w, h) * 0.25;
  const QPointF center(w * 0.5, h * 0.5);

  QPen pen(palette().color(QPalette::WindowText));
  pen.setWidthF(1.8);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);

  painter.save();
  painter.translate(center);
  painter.rotate(90.0 * m_progress);

  QPolygonF points;
  points << QPointF(-size * 0.5, -size) << QPointF(size * 0.5, 0) << QPointF(-size * 0.5, size);

  painter.drawPolyline(points);
  painter.restore();
}
QSize ChevronToggle::sizeHint() const {
  return QSize(28, 28);
}
