#include "ToastNotification.h"

#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include "app/interfaces/ThemeService.h"

namespace {
constexpr int kBorderRadius  = 12;
constexpr int kPaddingH      = 24;
constexpr int kPaddingV      = 12;
constexpr int kTopMargin     = 32;
constexpr int kSlideDistance = 18;
constexpr int kAnimDuration  = 320;
constexpr int kFontSize      = 13;

const QColor kToastPrimaryTextColor(255, 255, 0);

QColor toastBackgroundColor(ThemeService* theme) {
  if (theme) {
    return theme->color("primary");
  }
  return QColor(90, 169, 255);
}

QColor toastBackgroundColor(ThemeService*           theme,
                            ToastNotification::Tone tone) {
  if (tone == ToastNotification::Tone::Error) {
    if (theme) {
      return theme->color("error");
    }
    return QColor(239, 68, 68);
  }
  return toastBackgroundColor(theme);
}

QColor toastTextColor(ThemeService*           theme,
                      ToastNotification::Tone tone) {
  if (tone == ToastNotification::Tone::Success) {
    Q_UNUSED(theme)
    return QColor(255, 255, 255);
  }
  return kToastPrimaryTextColor;
}
}  // namespace

void ToastNotification::show(QWidget*       parent,
                             const QString& message,
                             ThemeService*  theme,
                             int            durationMs,
                             Tone           tone) {
  if (!parent) {
    return;
  }
  auto* toast = new ToastNotification(parent, message, theme, durationMs, tone);
  toast->startShowAnimation();
}

ToastNotification::ToastNotification(QWidget*       parent,
                                     const QString& message,
                                     ThemeService*  theme,
                                     int            durationMs,
                                     Tone           tone)
    : QWidget(parent),
      m_message(message),
      m_theme(theme),
      m_tone(tone),
      m_duration(durationMs) {
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_TranslucentBackground);
  setWindowFlags(Qt::Widget);

  QFont font;
  font.setPixelSize(kFontSize);
  font.setWeight(QFont::Bold);
  QFontMetrics metrics(font);
  const int    textWidth  = metrics.horizontalAdvance(m_message);
  const int    textHeight = metrics.height();
  resize(textWidth + kPaddingH * 2, textHeight + kPaddingV * 2);

  reposition();
  parent->installEventFilter(this);

  m_hideTimer.setSingleShot(true);
  connect(&m_hideTimer, &QTimer::timeout, this,
          &ToastNotification::startHideAnimation);
}

qreal ToastNotification::opacity() const {
  return m_opacity;
}

void ToastNotification::setOpacity(qreal value) {
  m_opacity = value;
  update();
}

int ToastNotification::offsetY() const {
  return m_offsetY;
}

void ToastNotification::setOffsetY(int y) {
  m_offsetY = y;
  reposition();
}

void ToastNotification::paintEvent(QPaintEvent* /*event*/) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setOpacity(m_opacity);

  QPainterPath path;
  path.addRoundedRect(rect(), kBorderRadius, kBorderRadius);
  painter.fillPath(path, toastBackgroundColor(m_theme, m_tone));

  QFont font = painter.font();
  font.setPixelSize(kFontSize);
  font.setWeight(QFont::Bold);
  painter.setFont(font);
  painter.setPen(toastTextColor(m_theme, m_tone));
  painter.drawText(rect(), Qt::AlignCenter, m_message);
}

bool ToastNotification::eventFilter(QObject* obj, QEvent* event) {
  if (obj == parent() && event->type() == QEvent::Resize) {
    reposition();
  }
  return QWidget::eventFilter(obj, event);
}

void ToastNotification::reposition() {
  auto* parentWidget = this->parentWidget();
  if (!parentWidget) {
    return;
  }
  const int x = (parentWidget->width() - width()) / 2;
  const int y = kTopMargin + m_offsetY;
  move(x, y);
}

void ToastNotification::startShowAnimation() {
  QWidget::show();
  raise();

  auto* group = new QParallelAnimationGroup(this);

  auto* fadeIn = new QPropertyAnimation(this, "opacity", this);
  fadeIn->setDuration(kAnimDuration);
  fadeIn->setStartValue(0.0);
  fadeIn->setEndValue(1.0);
  fadeIn->setEasingCurve(QEasingCurve::OutCubic);
  group->addAnimation(fadeIn);

  auto* slide = new QPropertyAnimation(this, "offsetY", this);
  slide->setDuration(kAnimDuration);
  slide->setStartValue(-kSlideDistance);
  slide->setEndValue(0);
  slide->setEasingCurve(QEasingCurve::OutCubic);
  group->addAnimation(slide);

  group->start(QAbstractAnimation::DeleteWhenStopped);
  m_hideTimer.start(m_duration);
}

void ToastNotification::startHideAnimation() {
  auto* group = new QParallelAnimationGroup(this);

  auto* fadeOut = new QPropertyAnimation(this, "opacity", this);
  fadeOut->setDuration(kAnimDuration);
  fadeOut->setStartValue(m_opacity);
  fadeOut->setEndValue(0.0);
  fadeOut->setEasingCurve(QEasingCurve::InCubic);
  group->addAnimation(fadeOut);

  auto* slide = new QPropertyAnimation(this, "offsetY", this);
  slide->setDuration(kAnimDuration);
  slide->setStartValue(m_offsetY);
  slide->setEndValue(-kSlideDistance);
  slide->setEasingCurve(QEasingCurve::InCubic);
  group->addAnimation(slide);

  connect(group, &QAbstractAnimation::finished, this, &QWidget::deleteLater);
  group->start(QAbstractAnimation::DeleteWhenStopped);
}
