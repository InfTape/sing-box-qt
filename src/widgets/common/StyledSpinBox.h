#ifndef STYLEDSPINBOX_H
#define STYLEDSPINBOX_H
#include <QSpinBox>

class StyledSpinBox : public QSpinBox {
  Q_OBJECT
 public:
  explicit StyledSpinBox(QWidget* parent = nullptr);

 protected:
  void contextMenuEvent(QContextMenuEvent* event) override;
};
#endif  // STYLEDSPINBOX_H
