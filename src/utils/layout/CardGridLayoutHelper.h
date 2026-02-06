#pragma once
class CardGridLayoutHelper {
 public:
  static constexpr int kCardWidth  = 280;
  static constexpr int kCardHeight = 200;
  static int           computeColumns(int availableWidth, int spacing);
  static int           computeHorizontalMargin(int availableWidth, int spacing, int columns);
};
