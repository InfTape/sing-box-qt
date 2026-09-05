#ifndef LOGROWWIDGET_H
#define LOGROWWIDGET_H
#include <QFrame>
#include "utils/LogParser.h"
class QLabel;

class LogRowWidget : public QFrame {
  Q_OBJECT
 public:
  explicit LogRowWidget(const LogParser::LogEntry& entry,
                        QWidget*                   parent = nullptr);
  void setEntry(const LogParser::LogEntry& entry);
  void reset();
 private:
  void setBadge(QLabel* badge, const QString& text);
  QLabel* m_time;
  QLabel* m_level;
  QLabel* m_direction;
  QLabel* m_network;
  QLabel* m_content;
  QString m_type;
};
#endif  // LOGROWWIDGET_H
