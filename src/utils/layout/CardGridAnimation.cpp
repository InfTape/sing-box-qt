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
    const QRect endRect = w->geometry();
    QRect startRect = oldGeometries.value(w, endRect);

    // 跳过从原点 (0,0) 开始的新卡片动画，直接放到目标位置
    // 新创建的卡片位置默认是 (0,0)，不应该从左上角滑入
    if (startRect.topLeft() == QPoint(0, 0) &&
        endRect.topLeft() != QPoint(0, 0)) {
      continue;
    }

    Q_UNUSED(columnChanged)
    if (startRect == endRect) continue;

    w->setGeometry(startRect);
    auto* anim =
        new QPropertyAnimation(static_cast<QObject*>(w), "geometry", group);
    anim->setDuration(260);
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
