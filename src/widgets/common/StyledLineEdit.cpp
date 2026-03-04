#include "StyledLineEdit.h"
#include <QContextMenuEvent>
#include <QMenu>
#include "widgets/common/RoundedMenu.h"

void StyledLineEdit::contextMenuEvent(QContextMenuEvent* event) {
  QMenu* stdMenu = createStandardContextMenu();
  if (!stdMenu) {
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
