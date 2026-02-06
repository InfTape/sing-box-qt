#include "utils/layout/CardGridLayoutHelper.h"

#include <QtGlobal>
int CardGridLayoutHelper::computeColumns(int availableWidth, int spacing) {
  return qMax(1, (availableWidth + spacing) / (kCardWidth + spacing));
}
int CardGridLayoutHelper::computeHorizontalMargin(int availableWidth, int spacing, int columns) {
  const int totalWidth = columns * kCardWidth + (columns - 1) * spacing;
  return qMax(0, (availableWidth - totalWidth) / 2);
}
