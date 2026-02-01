#pragma once

#include <QHash>
#include <QRect>
#include <QList>

class QWidget;
class SubscriptionCard;

namespace SubscriptionAnimation {
void animateReflow(QWidget *container,
                   const QList<SubscriptionCard*> &cards,
                   const QHash<SubscriptionCard*, QRect> &oldGeometries,
                   int previousColumns,
                   int newColumns);
}
