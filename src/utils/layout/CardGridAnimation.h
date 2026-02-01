#pragma once

#include <QHash>
#include <QList>
#include <QRect>

class QWidget;
namespace CardGridAnimation {
void animateReflow(QWidget* container, const QList<QWidget*>& widgets,
                   const QHash<QWidget*, QRect>& oldGeometries,
                   int previousColumns, int newColumns);
}
