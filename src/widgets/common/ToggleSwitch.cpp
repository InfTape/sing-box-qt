#include "widgets/common/ToggleSwitch.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QtMath>
#include <algorithm>

#include "app/interfaces/ThemeService.h"
ToggleSwitch::ToggleSwitch(QWidget* parent, ThemeService* themeService)
    : QWidget(parent), m_themeService(themeService) {
  setCursor(Qt::PointingHandCursor);
  setFocusPolicy(Qt::StrongFocus);
  setMinimumSize(sizeHint());

  m_anim = new QPropertyAnimation(this, "offset", this);
  m_anim->setDuration(140);
  m_anim->setEasingCurve(QEasingCurve::InOutCubic);

  if (m_themeService) {
    connect(m_themeService, &ThemeService::themeChanged, this, qOverload<>(&ToggleSwitch::update));
  }
}
void ToggleSwitch::setChecked(bool checked) {
  if (m_checked == checked) return;
  animateTo(checked);
  setCheckedInternal(checked, true);
}
void ToggleSwitch::setCheckedInternal(bool checked, bool emitSignal) {
  m_checked = checked;
  if (emitSignal) emit toggled(m_checked);
  update();
}
void ToggleSwitch::setOffset(qreal v) {
  v = std::clamp(v, 0.0, 1.0);
  if (qFuzzyCompare(m_offset, v)) return;
  m_offset = v;
  update();
}
QRectF ToggleSwitch::trackRect() const {
  QRectF      r = rect().adjusted(2, 2, -2, -2);  // Leave padding to avoid clipping
  const qreal h = r.height() * 0.70;
  const qreal w = qMax(h * 1.8, r.width() * 0.90);
  const qreal x = r.center().x() - w / 2.0;
  const qreal y = r.center().y() - h / 2.0;
  return QRectF(x, y, w, h);
}
QRectF ToggleSwitch::thumbRectForOffset(qreal off) const {
  QRectF      tr     = trackRect();
  const qreal margin = tr.height() * 0.12;
  const qreal d      = tr.height() - 2 * margin;
  const qreal x0     = tr.left() + margin;
  const qreal x1     = tr.right() - margin - d;
  const qreal x      = x0 + (x1 - x0) * off;
  const qreal y      = tr.top() + margin;
  return QRectF(x, y, d, d);
}
void ToggleSwitch::animateTo(bool checked) {
  m_anim->stop();
  m_anim->setStartValue(m_offset);
  m_anim->setEndValue(checked ? 1.0 : 0.0);
  m_anim->start();
}
void ToggleSwitch::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  const QRectF tr = trackRect();
  const QRectF th = thumbRectForOffset(m_offset);

  // Colors from theme
  ThemeService* ts = m_themeService;
  const bool    en = isEnabled();

  // Use text-tertiary for day mode only, fallback to gray for dark mode
  const ThemeService::ThemeMode mode    = ts ? ts->themeMode() : ThemeService::ThemeMode::Dark;
  const bool                    isLight = mode == ThemeService::ThemeMode::Light ||
                       (mode == ThemeService::ThemeMode::Auto && ts && ts->color("bg-primary") == QColor("#f8fafc"));
  QColor trackOff = (isLight && ts) ? ts->color("text-tertiary") : QColor(120, 120, 120);

  QColor trackOn = ts ? ts->color("primary") : QColor(0, 0, 200);
  QColor thumb(255, 255, 255);

  QColor track = m_checked ? trackOn : trackOff;
  if (!en) {
    track.setAlpha(80);
    thumb.setAlpha(120);
  }

  p.setPen(Qt::NoPen);
  p.setBrush(track);
  p.drawRoundedRect(tr, tr.height() / 2.0, tr.height() / 2.0);

  if (en) {
    QColor shadow(0, 0, 0, 40);
    QRectF sh = th.translated(0, 1.2);
    p.setBrush(shadow);
    p.drawEllipse(sh);
  }

  p.setBrush(thumb);
  p.drawEllipse(th);

  if (hasFocus() && en) {
    QColor ringColor = track;
    ringColor.setAlpha(120);
    QPen pen(ringColor);
    pen.setWidthF(2.0);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QRectF ring = tr.adjusted(-2, -2, 2, 2);
    p.drawRoundedRect(ring, ring.height() / 2.0, ring.height() / 2.0);
  }
}
void ToggleSwitch::mousePressEvent(QMouseEvent* e) {
  if (!isEnabled() || e->button() != Qt::LeftButton) return;
  m_dragging    = true;
  m_pressPos    = e->pos();
  m_pressOffset = m_offset;
  m_anim->stop();
  e->accept();
}
void ToggleSwitch::mouseMoveEvent(QMouseEvent* e) {
  if (!m_dragging) return;

  QRectF      tr     = trackRect();
  const qreal margin = tr.height() * 0.12;
  const qreal d      = tr.height() - 2 * margin;
  const qreal x0     = tr.left() + margin;
  const qreal x1     = tr.right() - margin - d;

  const qreal dx    = e->pos().x() - m_pressPos.x();
  const qreal range = (x1 - x0);
  qreal       off   = m_pressOffset + (range > 1e-6 ? dx / range : 0.0);
  setOffset(off);

  const bool preview = (m_offset >= 0.5);
  if (preview != m_checked) {
    m_checked = preview;
    update();
  }

  e->accept();
}
void ToggleSwitch::mouseReleaseEvent(QMouseEvent* e) {
  if (!m_dragging || e->button() != Qt::LeftButton) return;
  m_dragging = false;

  const int moved  = (e->pos() - m_pressPos).manhattanLength();
  bool      target = m_checked;
  if (moved < 4) target = !m_checked;

  animateTo(target);
  setCheckedInternal(target, true);
  e->accept();
}
void ToggleSwitch::keyPressEvent(QKeyEvent* e) {
  if (!isEnabled()) return;
  if (e->key() == Qt::Key_Space || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
    setChecked(!m_checked);
    e->accept();
    return;
  }
  QWidget::keyPressEvent(e);
}
