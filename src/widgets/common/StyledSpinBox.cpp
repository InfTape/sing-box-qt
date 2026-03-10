#include "StyledSpinBox.h"
#include <QContextMenuEvent>
#include <QLineEdit>
#include <QMenu>
#include "widgets/common/RoundedMenu.h"
#include "widgets/common/StyledLineEdit.h"

StyledSpinBox::StyledSpinBox(QWidget* parent) : QSpinBox(parent) {
  setLineEdit(new StyledLineEdit(this));
}

void StyledSpinBox::contextMenuEvent(QContextMenuEvent* event) {
  QLineEdit* le      = lineEdit();
  QMenu*     stdMenu = le ? le->createStandardContextMenu() : nullptr;
  if (!stdMenu) {
    QSpinBox::contextMenuEvent(event);
    return;
  }
  RoundedMenu menu(this);
  menu.setObjectName("ComboMenu");
  for (QAction* action : stdMenu->actions()) {
    if (action->isSeparator()) {
      menu.addSeparator();
    } else {
      QAction* a = menu.addAction(action->icon(), action->text());
      a->setEnabled(action->isEnabled());
      a->setShortcut(action->shortcut());
      connect(a, &QAction::triggered, action, &QAction::trigger);
    }
  }
  menu.exec(event->globalPos());
  delete stdMenu;
}
