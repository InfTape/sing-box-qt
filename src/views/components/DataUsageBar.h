#ifndef DATAUSAGEBAR_H
#define DATAUSAGEBAR_H
#include <QProgressBar>

class DataUsageBar : public QProgressBar {
  Q_OBJECT
 public:
  explicit DataUsageBar(QWidget* parent = nullptr);
  void setLogScaledValue(qint64 total, qint64 maxTotal);

 private:
  static int calculateLogScaledValue(qint64 total, qint64 maxTotal);
};
#endif  // DATAUSAGEBAR_H
