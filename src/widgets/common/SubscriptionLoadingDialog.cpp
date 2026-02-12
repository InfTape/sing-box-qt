#include "widgets/common/SubscriptionLoadingDialog.h"
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QTimer>
#include <QWidget>

namespace {
constexpr int kDialogWidth  = 220;
constexpr int kDialogHeight = 180;
}  // namespace

SubscriptionLoadingDialog::SubscriptionLoadingDialog(QWidget* parent)
    : QDialog(parent), m_spinTimer(new QTimer(this)), m_closeTimer(new QTimer(this)) {
  setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::CustomizeWindowHint);
  setWindowModality(Qt::ApplicationModal);
  setAttribute(Qt::WA_TranslucentBackground);
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

  const QRectF panelRect = rect().adjusted(10, 10, -10, -10);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(12, 16, 22, 235));
  painter.drawRoundedRect(panelRect, 12, 12);

  const QPointF center(panelRect.center().x(), panelRect.center().y() - 16);
  const QRectF  iconRect(center.x() - 26, center.y() - 26, 52, 52);

  if (m_state == State::Loading) {
    QPen basePen(QColor(255, 255, 255, 40));
    basePen.setWidth(6);
    basePen.setCapStyle(Qt::RoundCap);
    painter.setPen(basePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(iconRect);

    QPen spinPen(QColor(94, 214, 255));
    spinPen.setWidth(6);
    spinPen.setCapStyle(Qt::RoundCap);
    painter.setPen(spinPen);
    painter.drawArc(iconRect, (-m_angle + 90) * 16, -120 * 16);

    painter.setPen(QColor(225, 232, 240));
    painter.setFont(QFont("Segoe UI", 10));
    painter.drawText(panelRect.adjusted(0, 95, 0, 0),
                     Qt::AlignHCenter | Qt::AlignTop,
                     tr("Downloading..."));
    return;
  }

  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(98, 247, 72));
  painter.drawEllipse(iconRect);

  QPen checkPen(QColor(14, 38, 21));
  checkPen.setWidth(5);
  checkPen.setCapStyle(Qt::RoundCap);
  checkPen.setJoinStyle(Qt::RoundJoin);
  painter.setPen(checkPen);
  QPainterPath checkPath;
  checkPath.moveTo(iconRect.left() + 14, iconRect.top() + 28);
  checkPath.lineTo(iconRect.left() + 23, iconRect.top() + 36);
  checkPath.lineTo(iconRect.right() - 13, iconRect.top() + 16);
  painter.drawPath(checkPath);

  painter.setPen(QColor(225, 232, 240));
  painter.setFont(QFont("Segoe UI", 10));
  painter.drawText(panelRect.adjusted(0, 95, 0, 0),
                   Qt::AlignHCenter | Qt::AlignTop,
                   tr("Done"));
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
