#ifndef STYLEDTEXTEDIT_H
#define STYLEDTEXTEDIT_H
#include <QTextEdit>

class StyledTextEdit : public QTextEdit {
  Q_OBJECT
 public:
  using QTextEdit::QTextEdit;

 protected:
  void contextMenuEvent(QContextMenuEvent* event) override;
};
#endif  // STYLEDTEXTEDIT_H
