#include "DataUsageBar.h"
#include <QtMath>

DataUsageBar::DataUsageBar(QWidget* parent) : QProgressBar(parent) {
  setRange(0, 1000);
  setTextVisible(false);
  setFixedHeight(10);
}

void DataUsageBar::setScaledValue(qint64 total, qint64 maxTotal) {
  setValue(calculateScaledValue(total, maxTotal));
}

int DataUsageBar::calculateScaledValue(qint64 total, qint64 maxTotal) {
  if (total <= 0 || maxTotal <= 0) {
    return 0;
  }

  double ratio = static_cast<double>(total) / static_cast<double>(maxTotal);
  ratio        = qBound(0.0, ratio, 1.0);
  if (ratio <= 0.0) {
    return 0;
  }

  // Power-curve mapping: ratio^0.4 gives good visual separation
  // between top and tail entries without over-compressing differences.
  constexpr double kExponent  = 0.4;
  const double     emphasized = qPow(ratio, kExponent);
  int              value      = static_cast<int>(emphasized * 1000.0 + 0.5);

  // Keep non-zero entries minimally visible.
  if (value <= 0 && total > 0) {
    value = 12;
  }
  return qBound(0, value, 1000);
}
