#include "utils/layout/CardGridAnimation.h"

#include <QAbstractAnimation>
#include <QEasingCurve>
#include <QObject>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QWidget>
namespace CardGridAnimation {
void animateReflow(QWidget* container, const QList<QWidget*>& widgets,
                   const QHash<QWidget*, QRect>& oldGeometries,
                   int previousColumns, int newColumns) {
  if (!container) return;

  const bool columnChanged =
      previousColumns > 0 && previousColumns != newColumns;
  auto* group = new QParallelAnimationGroup(static_cast<QObject*>(container));

  for (QWidget* w : widgets) {
    if (!w) continue;
    const QRect endRect   = w->geometry();
    QRect       startRect = oldGeometries.value(w, endRect);

    if (startRect == endRect && columnChanged) {
      const int offsetX = newColumns > previousColumns ? 18 : -18;
      startRect.translate(offsetX, 12);
    }
    if (startRect == endRect) continue;

    w->setGeometry(startRect);
    auto* anim =
        new QPropertyAnimation(static_cast<QObject*>(w), "geometry", group);
    anim->setDuration(220);
    anim->setEasingCurve(QEasingCurve::OutSine);
    anim->setStartValue(startRect);
    anim->setEndValue(endRect);
    QObject::connect(w, &QObject::destroyed, anim, [anim]() {
      if (anim->state() != QAbstractAnimation::Stopped) anim->stop();
    });
    group->addAnimation(anim);
  }

  if (group->animationCount() > 0) {
    group->start(QAbstractAnimation::DeleteWhenStopped);
  } else {
    group->deleteLater();
  }
}
}  // namespace CardGridAnimation
