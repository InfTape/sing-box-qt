#include "ProxyTreePresenter.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QSizePolicy>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include "views/proxy/ProxyTreeUtils.h"
#include "widgets/common/ChevronToggle.h"
namespace {
QString normalizeNodeName(QString name) {
  if (name.startsWith("* ")) {
    name = name.mid(2);
  }
  return name;
}
QString asCountText(const ProxyTreePresenter::CountFormatter& formatter, int count) {
  if (formatter) return formatter(count);
  return QString("%1 nodes").arg(count);
}
QString asCurrentText(const ProxyTreePresenter::CurrentFormatter& formatter, const QString& proxy) {
  if (formatter) return formatter(proxy);
  return QString("Current: %1").arg(proxy);
}
QString asDelayText(const ProxyTreePresenter::DelayFormatter& formatter, int delay) {
  if (formatter) return formatter(delay);
  if (delay <= 0) return QString("Timeout");
  return QString::number(delay) + " ms";
}
}  // namespace
ProxyTreePresenter::ProxyTreePresenter(QTreeWidget* treeWidget) : m_treeWidget(treeWidget) {}
void ProxyTreePresenter::setTreeWidget(QTreeWidget* treeWidget) {
  m_treeWidget = treeWidget;
}
QJsonObject ProxyTreePresenter::render(const QJsonObject& proxies, const DelayFormatter& formatDelay,
                                       const CountFormatter&   formatNodeCount,
                                       const CurrentFormatter& formatCurrent) const {
  if (!m_treeWidget) return QJsonObject();
  QSet<QString>           expandedGroups;
  QString                 selectedNode;
  QTreeWidgetItemIterator oldIt(m_treeWidget);
  while (*oldIt) {
    if ((*oldIt)->isExpanded()) {
      const QString groupName = (*oldIt)->data(0, Qt::UserRole + 1).toString();
      expandedGroups.insert(groupName.isEmpty() ? (*oldIt)->text(0) : groupName);
    }
    if ((*oldIt)->isSelected()) {
      selectedNode = normalizeNodeName(ProxyTreeUtils::nodeDisplayName(*oldIt));
    }
    ++oldIt;
  }
  m_treeWidget->clear();
  const QJsonObject cachedProxies = proxies.value("proxies").toObject();
  for (auto it = cachedProxies.begin(); it != cachedProxies.end(); ++it) {
    const QJsonObject proxy = it.value().toObject();
    const QString     type  = proxy.value("type").toString();
    if (type != "Selector" && type != "URLTest" && type != "Fallback") {
      continue;
    }
    auto* groupItem = new QTreeWidgetItem(m_treeWidget);
    groupItem->setText(0, QString());
    groupItem->setText(1, type);
    groupItem->setData(0, Qt::UserRole, "group");
    groupItem->setData(0, Qt::UserRole + 1, it.key());
    groupItem->setFlags(groupItem->flags() & ~Qt::ItemIsSelectable);
    groupItem->setFirstColumnSpanned(true);
    QFont font = groupItem->font(0);
    font.setBold(true);
    groupItem->setFont(0, font);
    if (expandedGroups.contains(it.key())) {
      groupItem->setExpanded(true);
    }
    const QJsonArray all       = proxy.value("all").toArray();
    const QString    now       = proxy.value("now").toString();
    auto*            groupCard = new QFrame(m_treeWidget->viewport());
    groupCard->setObjectName("ProxyGroupCard");
    auto* cardLayout = new QHBoxLayout(groupCard);
    cardLayout->setContentsMargins(14, 12, 14, 12);
    cardLayout->setSpacing(10);
    auto* titleLabel = new QLabel(it.key(), groupCard);
    titleLabel->setObjectName("ProxyGroupTitle");
    auto* typeLabel = new QLabel(type, groupCard);
    typeLabel->setAlignment(Qt::AlignCenter);
    typeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto* countLabel = new QLabel(asCountText(formatNodeCount, all.size()), groupCard);
    countLabel->setObjectName("ProxyGroupMeta");
    auto* currentLabel = new QLabel(now.isEmpty() ? QString() : asCurrentText(formatCurrent, now), groupCard);
    currentLabel->setObjectName("ProxyGroupCurrent");
    currentLabel->setVisible(!now.isEmpty());
    cardLayout->addWidget(titleLabel);
    cardLayout->addWidget(typeLabel);
    cardLayout->addSpacing(6);
    cardLayout->addWidget(countLabel);
    cardLayout->addSpacing(6);
    cardLayout->addWidget(currentLabel);
    cardLayout->addStretch();
    auto* toggleBtn = new ChevronToggle(groupCard);
    toggleBtn->setObjectName("ProxyGroupToggle");
    toggleBtn->setExpanded(groupItem->isExpanded());
    toggleBtn->setFixedSize(28, 28);
    cardLayout->addWidget(toggleBtn);
    groupItem->setSizeHint(0, QSize(0, 72));
    groupItem->setText(1, QString());
    groupItem->setText(2, QString());
    m_treeWidget->setItemWidget(groupItem, 0, groupCard);
    QObject::connect(toggleBtn, &ChevronToggle::toggled,
                     [groupItem](bool expanded) { groupItem->setExpanded(expanded); });
    for (const auto& nodeName : all) {
      const QString name     = nodeName.toString();
      auto*         nodeItem = new QTreeWidgetItem(groupItem);
      nodeItem->setText(0, QString());
      nodeItem->setText(1, QString());
      nodeItem->setText(2, QString());
      nodeItem->setFirstColumnSpanned(true);
      nodeItem->setData(0, Qt::UserRole, "node");
      nodeItem->setData(0, Qt::UserRole + 1, it.key());
      nodeItem->setData(0, Qt::UserRole + 3, name);
      if (name == selectedNode) {
        nodeItem->setSelected(true);
      }
      QString nodeType;
      QString delayText;
      if (cachedProxies.contains(name)) {
        const QJsonObject nodeProxy = cachedProxies.value(name).toObject();
        nodeType                    = nodeProxy.value("type").toString();
        if (nodeProxy.contains("history")) {
          const QJsonArray history = nodeProxy.value("history").toArray();
          if (!history.isEmpty()) {
            const int delay = history.last().toObject().value("delay").toInt();
            delayText       = asDelayText(formatDelay, delay);
          }
        }
      }
      nodeItem->setText(2, delayText);
      QWidget* rowCard = ProxyTreeUtils::buildNodeRow(m_treeWidget, name, nodeType, delayText);
      nodeItem->setSizeHint(0, rowCard->sizeHint());
      m_treeWidget->setItemWidget(nodeItem, 0, rowCard);
      ProxyTreeUtils::updateNodeRowSelected(m_treeWidget, nodeItem, nodeItem->isSelected());
    }
  }
  ProxyTreeUtils::applyTreeItemColors(m_treeWidget, cachedProxies);
  return cachedProxies;
}
void ProxyTreePresenter::updateSelectedProxy(QJsonObject& cachedProxies, const QString& group, const QString& proxy,
                                             const CurrentFormatter& formatCurrent) const {
  if (group.isEmpty() || proxy.isEmpty() || !m_treeWidget) return;
  if (cachedProxies.contains(group)) {
    QJsonObject groupProxy = cachedProxies.value(group).toObject();
    groupProxy["now"]      = proxy;
    cachedProxies[group]   = groupProxy;
  }
  QTreeWidgetItem* groupItem = nullptr;
  for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
    auto* item = m_treeWidget->topLevelItem(i);
    if (item && item->data(0, Qt::UserRole + 1).toString() == group) {
      groupItem = item;
      break;
    }
  }
  if (!groupItem) return;
  ProxyTreeUtils::updateGroupCurrentLabel(m_treeWidget, groupItem, asCurrentText(formatCurrent, proxy));
  for (int i = 0; i < groupItem->childCount(); ++i) {
    QTreeWidgetItem* child = groupItem->child(i);
    if (!child) continue;
    ProxyTreeUtils::markNodeState(m_treeWidget, child, proxy, child->text(2));
  }
}
