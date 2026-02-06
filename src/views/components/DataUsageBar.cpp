#include "DataUsageBar.h"
#include <QtMath>

DataUsageBar::DataUsageBar(QWidget* parent) : QProgressBar(parent) {
  setRange(0, 1000);
  setTextVisible(false);
  setFixedHeight(10);
}

void DataUsageBar::setLogScaledValue(qint64 total, qint64 maxTotal) {
  setValue(calculateLogScaledValue(total, maxTotal));
}

int DataUsageBar::calculateLogScaledValue(qint64 total, qint64 maxTotal) {
  if (total <= 0 || maxTotal <= 0)
    return 0;
  const double maxLog = qLn(static_cast<double>(maxTotal) + 1.0);
  if (maxLog <= 0)
    return 0;
  const double currentLog = qLn(static_cast<double>(total) + 1.0);
  const int    value      = static_cast<int>(currentLog * 1000.0 / maxLog);
  if (value < 0)
    return 0;
  if (value > 1000)
    return 1000;
  return value;
}
