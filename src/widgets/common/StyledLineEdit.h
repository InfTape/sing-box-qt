#ifndef STYLEDLINEEDIT_H
#define STYLEDLINEEDIT_H
#include <QLineEdit>

class StyledLineEdit : public QLineEdit {
  Q_OBJECT
 public:
  using QLineEdit::QLineEdit;

 protected:
  void contextMenuEvent(QContextMenuEvent* event) override;
};
#endif  // STYLEDLINEEDIT_H
