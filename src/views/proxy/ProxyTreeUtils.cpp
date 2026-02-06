#include "ProxyTreeUtils.h"
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include "utils/proxy/ProxyNodeHelper.h"

namespace ProxyTreeUtils {
QString nodeDisplayName(QTreeWidgetItem* item) {
  if (!item) {
    return QString();
  }
  QString name = item->data(0, Qt::UserRole + 3).toString();
  if (name.isEmpty()) {
    name = item->text(0);
  }
  return name;
}

QWidget* buildNodeRow(QTreeWidget*   treeWidget,
                      const QString& name,
                      const QString& type,
                      const QString& delay) {
  QWidget* card = new QFrame(treeWidget ? treeWidget->viewport() : nullptr);
  card->setObjectName("ProxyNodeCard");
  card->setAttribute(Qt::WA_Hover, true);
  card->setMouseTracking(true);
  card->setProperty("selected", false);
  auto* layout = new QHBoxLayout(card);
  layout->setContentsMargins(14, 10, 14, 10);
  layout->setSpacing(10);
  auto* nameLabel = new QLabel(name, card);
  nameLabel->setObjectName("ProxyNodeName");
  auto* typeLabel = new QLabel(type, card);
  typeLabel->setObjectName("ProxyNodeType");
  typeLabel->setAlignment(Qt::AlignCenter);
  typeLabel->setMinimumWidth(64);
  auto* delayLabel = new QLabel(delay, card);
  delayLabel->setObjectName("ProxyNodeDelay");
  delayLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  layout->addWidget(nameLabel, 1);
  layout->addWidget(typeLabel);
  layout->addStretch();
  layout->addWidget(delayLabel, 0, Qt::AlignRight);
  return card;
}

void updateNodeRowDelay(QTreeWidget*     treeWidget,
                        QTreeWidgetItem* item,
                        const QString&   delayText,
                        const QString&   state) {
  if (!treeWidget || !item) {
    return;
  }
  QWidget* row = treeWidget->itemWidget(item, 0);
  if (!row) {
    return;
  }
  if (auto* delayLabel = row->findChild<QLabel*>("ProxyNodeDelay")) {
    delayLabel->setText(delayText);
    delayLabel->setProperty("state", state);
    delayLabel->style()->unpolish(delayLabel);
    delayLabel->style()->polish(delayLabel);
  }
}

void updateNodeRowSelected(QTreeWidget*     treeWidget,
                           QTreeWidgetItem* item,
                           bool             selected) {
  if (!treeWidget || !item) {
    return;
  }
  QWidget* row = treeWidget->itemWidget(item, 0);
  if (!row) {
    return;
  }
  row->setProperty("selected", selected);
  row->style()->unpolish(row);
  row->style()->polish(row);
}

void markNodeState(QTreeWidget*     treeWidget,
                   QTreeWidgetItem* item,
                   const QString&   now,
                   const QString&   delayText) {
  if (!treeWidget || !item) {
    return;
  }
  QString baseName = item->data(0, Qt::UserRole + 3).toString();
  if (baseName.isEmpty()) {
    baseName = item->text(0);
  }
  if (baseName.startsWith("* ")) {
    baseName = baseName.mid(2);
  }
  const bool isActive = (baseName == now);
  item->setData(0, Qt::UserRole + 2, isActive ? "active" : "");
  const QString displayName = isActive ? "* " + baseName : baseName;
  const QString state       = ProxyNodeHelper::delayStateFromText(delayText);
  item->setData(2, Qt::UserRole + 2, state);
  if (QWidget* row = treeWidget->itemWidget(item, 0)) {
    if (auto* nameLabel = row->findChild<QLabel*>("ProxyNodeName")) {
      nameLabel->setText(displayName);
    }
  }
  updateNodeRowDelay(treeWidget, item, delayText, state);
  updateNodeRowSelected(treeWidget, item, item->isSelected());
}

void applyTreeItemColors(QTreeWidget* treeWidget, const QJsonObject& proxies) {
  if (!treeWidget) {
    return;
  }
  QTreeWidgetItemIterator it(treeWidget);
  while (*it) {
    QTreeWidgetItem* item = *it;
    const QString    role = item->data(0, Qt::UserRole).toString();
    if (role == "node") {
      const QString group = item->data(0, Qt::UserRole + 1).toString();
      const QString now =
          proxies.value(group).toObject().value("now").toString();
      markNodeState(treeWidget, item, now, item->text(2));
    }
    ++it;
  }
}

void filterTreeNodes(QTreeWidget* treeWidget, const QString& text) {
  if (!treeWidget) {
    return;
  }
  QTreeWidgetItemIterator it(treeWidget);
  while (*it) {
    if ((*it)->data(0, Qt::UserRole).toString() == "node") {
      const bool match =
          nodeDisplayName(*it).contains(text, Qt::CaseInsensitive);
      (*it)->setHidden(!match && !text.isEmpty());
    }
    ++it;
  }
  if (text.isEmpty()) {
    return;
  }
  it = QTreeWidgetItemIterator(treeWidget);
  while (*it) {
    if ((*it)->data(0, Qt::UserRole).toString() == "group") {
      bool hasVisibleChild = false;
      for (int i = 0; i < (*it)->childCount(); ++i) {
        if (!(*it)->child(i)->isHidden()) {
          hasVisibleChild = true;
          break;
        }
      }
      (*it)->setExpanded(hasVisibleChild);
      (*it)->setHidden(!hasVisibleChild);
    }
    ++it;
  }
}

void updateGroupCurrentLabel(QTreeWidget*     treeWidget,
                             QTreeWidgetItem* groupItem,
                             const QString&   text) {
  if (!treeWidget || !groupItem) {
    return;
  }
  QWidget* groupCard = treeWidget->itemWidget(groupItem, 0);
  if (!groupCard) {
    return;
  }
  QLabel* currentLabel = groupCard->findChild<QLabel*>("ProxyGroupCurrent");
  if (!currentLabel) {
    return;
  }
  currentLabel->setText(text);
  currentLabel->setVisible(true);
}
}  // namespace ProxyTreeUtils
