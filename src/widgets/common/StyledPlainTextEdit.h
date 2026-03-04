#ifndef STYLEDPLAINTEXTEDIT_H
#define STYLEDPLAINTEXTEDIT_H
#include <QPlainTextEdit>

class StyledPlainTextEdit : public QPlainTextEdit {
  Q_OBJECT
 public:
  using QPlainTextEdit::QPlainTextEdit;

 protected:
  void contextMenuEvent(QContextMenuEvent* event) override;
};
#endif  // STYLEDPLAINTEXTEDIT_H
