#ifndef LOGROWWIDGET_H
#define LOGROWWIDGET_H

#include <QFrame>

#include "utils/LogParser.h"
class LogRowWidget : public QFrame {
  Q_OBJECT

 public:
  explicit LogRowWidget(const LogParser::LogEntry& entry, QWidget* parent = nullptr);
};
#endif  // LOGROWWIDGET_H
