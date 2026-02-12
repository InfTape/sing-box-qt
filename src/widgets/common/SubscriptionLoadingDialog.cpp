#include "widgets/common/SubscriptionLoadingDialog.h"
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QTimer>
#include <QWidget>

namespace {
constexpr int kDialogWidth  = 160;
constexpr int kDialogHeight = 160;
constexpr int kIconSize     = 70;
constexpr int kStrokeWidth  = 8;
const QColor  kAccentGreen(105, 224, 118);
const QColor  kTrackGreen(168, 236, 177);
}  // namespace

SubscriptionLoadingDialog::SubscriptionLoadingDialog(QWidget* parent)
    : QDialog(parent), m_spinTimer(new QTimer(this)), m_closeTimer(new QTimer(this)) {
  setObjectName("SubscriptionLoadingDialog");
  setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::CustomizeWindowHint);
  setWindowModality(Qt::ApplicationModal);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_NoSystemBackground);
  setStyleSheet("background: transparent;");
  setFixedSize(kDialogWidth, kDialogHeight);

  m_spinTimer->setInterval(16);
  connect(m_spinTimer, &QTimer::timeout, this, [this]() {
    m_angle = (m_angle + 8) % 360;
    update();
  });

  m_closeTimer->setSingleShot(true);
  connect(m_closeTimer, &QTimer::timeout, this, &QDialog::close);
}

void SubscriptionLoadingDialog::showLoading(QWidget* anchor) {
  m_state = State::Loading;
  m_angle = 0;
  m_closeTimer->stop();
  m_spinTimer->start();
  centerToAnchor(anchor);
  show();
  raise();
  update();
}

void SubscriptionLoadingDialog::showSuccessAndAutoClose(int msecs) {
  m_state = State::Success;
  m_spinTimer->stop();
  update();
  m_closeTimer->start(msecs);
}

void SubscriptionLoadingDialog::closePopup() {
  m_spinTimer->stop();
  m_closeTimer->stop();
  close();
}

void SubscriptionLoadingDialog::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event)

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const QRectF panelRect = rect();
  const QPointF center(panelRect.center().x(), panelRect.center().y());
  const qreal  half = kIconSize / 2.0;
  const QRectF iconRect(center.x() - half, center.y() - half, kIconSize, kIconSize);

  if (m_state == State::Loading) {
    QPen basePen(kTrackGreen);
    basePen.setWidth(kStrokeWidth);
    basePen.setCapStyle(Qt::RoundCap);
    painter.setPen(basePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(iconRect);

    QPen spinPen(kAccentGreen);
    spinPen.setWidth(kStrokeWidth);
    spinPen.setCapStyle(Qt::RoundCap);
    painter.setPen(spinPen);
    painter.drawArc(iconRect, (-m_angle + 120) * 16, -96 * 16);
    return;
  }

  QPen circlePen(kAccentGreen);
  circlePen.setWidth(kStrokeWidth - 1);
  circlePen.setCapStyle(Qt::RoundCap);
  painter.setPen(circlePen);
  painter.setBrush(Qt::NoBrush);
  painter.drawEllipse(iconRect);

  QPen checkPen(kAccentGreen);
  checkPen.setWidth(kStrokeWidth - 1);
  checkPen.setCapStyle(Qt::RoundCap);
  checkPen.setJoinStyle(Qt::RoundJoin);
  painter.setPen(checkPen);
  QPainterPath checkPath;
  checkPath.moveTo(iconRect.left() + 20, iconRect.top() + 39);
  checkPath.lineTo(iconRect.left() + 31, iconRect.top() + 51);
  checkPath.lineTo(iconRect.right() - 17, iconRect.top() + 24);
  painter.drawPath(checkPath);
}

void SubscriptionLoadingDialog::centerToAnchor(QWidget* anchor) {
  QWidget* target = anchor ? anchor : parentWidget();
  if (!target) {
    return;
  }
  const QPoint topLeft = target->mapToGlobal(QPoint(0, 0));
  const QRect  globalRect(topLeft, target->size());
  move(globalRect.center() - QPoint(width() / 2, height() / 2));
}
