#ifndef PROXYTREEUTILS_H
#define PROXYTREEUTILS_H
#include <QJsonObject>
#include <QString>
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;
namespace ProxyTreeUtils {
QString  nodeDisplayName(QTreeWidgetItem* item);
QWidget* buildNodeRow(QTreeWidget* treeWidget, const QString& name, const QString& type, const QString& delay);
void updateNodeRowDelay(QTreeWidget* treeWidget, QTreeWidgetItem* item, const QString& delayText, const QString& state);
void updateNodeRowSelected(QTreeWidget* treeWidget, QTreeWidgetItem* item, bool selected);
void markNodeState(QTreeWidget* treeWidget, QTreeWidgetItem* item, const QString& now,
                   const QString& delayText = QString());
void applyTreeItemColors(QTreeWidget* treeWidget, const QJsonObject& proxies);
void filterTreeNodes(QTreeWidget* treeWidget, const QString& text);
void updateGroupCurrentLabel(QTreeWidget* treeWidget, QTreeWidgetItem* groupItem, const QString& text);
}  // namespace ProxyTreeUtils
#endif  // PROXYTREEUTILS_H
