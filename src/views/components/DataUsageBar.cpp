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
  if (ratio < 0.0) {
    ratio = 0.0;
  } else if (ratio > 1.0) {
    ratio = 1.0;
  }
  if (ratio <= 0.0) {
    return 0;
  }

  // Contrast controls:
  // kContrast: larger -> head values stand out more.
  // gamma: larger -> tail compressed more.
  constexpr double kContrast = 16.0;
  constexpr double gamma     = 0.5;

  const double normalizedLog =
      qLn(1.0 + kContrast * ratio) / qLn(1.0 + kContrast);
  const double emphasized = qPow(normalizedLog, gamma);
  int          value      = static_cast<int>(emphasized * 1000.0 + 0.5);

  // Keep non-zero entries minimally visible.
  if (value <= 0 && total > 0) {
    value = 12;
  }
  if (value < 0) {
    return 0;
  }
  if (value > 1000) {
    return 1000;
  }
  return value;
}
