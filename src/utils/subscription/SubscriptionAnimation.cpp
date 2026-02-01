#include "utils/subscription/SubscriptionAnimation.h"
#include "views/subscription/SubscriptionCard.h"
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QEasingCurve>

namespace SubscriptionAnimation {

void animateReflow(QWidget *container,
                   const QList<SubscriptionCard*> &cards,
                   const QHash<SubscriptionCard*, QRect> &oldGeometries,
                   int previousColumns,
                   int newColumns)
{
    if (!container) return;

    const bool columnChanged = previousColumns > 0 && previousColumns != newColumns;
    auto *group = new QParallelAnimationGroup(container);

    for (SubscriptionCard *card : cards) {
        if (!card) continue;
        const QRect endRect = card->geometry();
        QRect startRect = oldGeometries.value(card, endRect);

        if (startRect == endRect && columnChanged) {
            const int offsetX = newColumns > previousColumns ? 18 : -18;
            startRect.translate(offsetX, 12);
        }
        if (startRect == endRect) continue;

        card->setGeometry(startRect);
        auto *anim = new QPropertyAnimation(card, "geometry", group);
        anim->setDuration(320);
        anim->setEasingCurve(QEasingCurve::OutBack);
        anim->setStartValue(startRect);
        anim->setEndValue(endRect);
        group->addAnimation(anim);
    }

    if (group->animationCount() > 0) {
        group->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        group->deleteLater();
    }
}

} // namespace SubscriptionAnimation
