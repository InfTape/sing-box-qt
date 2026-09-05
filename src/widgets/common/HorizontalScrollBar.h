#ifndef HORIZONTALSCROLLBAR_H
#define HORIZONTALSCROLLBAR_H
#include <QScrollBar>

class HorizontalScrollBar : public QScrollBar {
  Q_OBJECT
 public:
  explicit HorizontalScrollBar(QWidget* parent = nullptr);
};
#endif  // HORIZONTALSCROLLBAR_H
