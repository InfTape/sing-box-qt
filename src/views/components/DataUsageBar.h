#ifndef DATAUSAGEBAR_H
#define DATAUSAGEBAR_H
#include <QProgressBar>

class DataUsageBar : public QProgressBar {
  Q_OBJECT
 public:
  explicit DataUsageBar(QWidget* parent = nullptr);
  void setScaledValue(qint64 total, qint64 maxTotal);
  void setLogScaledValue(qint64 total, qint64 maxTotal) {
    setScaledValue(total, maxTotal);
  }

 private:
  static int calculateScaledValue(qint64 total, qint64 maxTotal);
};
#endif  // DATAUSAGEBAR_H
