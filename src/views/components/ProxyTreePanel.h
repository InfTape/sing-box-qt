#ifndef PROXYTREEPANEL_H
#define PROXYTREEPANEL_H
#include <QFrame>
class QTreeWidget;

class ProxyTreePanel : public QFrame {
  Q_OBJECT
 public:
  explicit ProxyTreePanel(QWidget* parent = nullptr);

  QTreeWidget* treeWidget() const { return m_treeWidget; }

 private:
  QTreeWidget* m_treeWidget = nullptr;
};
#endif  // PROXYTREEPANEL_H
