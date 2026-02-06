#ifndef PROXYTREEPRESENTER_H
#define PROXYTREEPRESENTER_H

#include <QJsonObject>
#include <QString>
#include <functional>

class QTreeWidget;

class ProxyTreePresenter {
 public:
  using DelayFormatter   = std::function<QString(int)>;
  using CountFormatter   = std::function<QString(int)>;
  using CurrentFormatter = std::function<QString(const QString&)>;

  explicit ProxyTreePresenter(QTreeWidget* treeWidget = nullptr);

  void setTreeWidget(QTreeWidget* treeWidget);

  QJsonObject render(const QJsonObject& proxies, const DelayFormatter& formatDelay,
                     const CountFormatter& formatNodeCount, const CurrentFormatter& formatCurrent) const;

  void updateSelectedProxy(QJsonObject& cachedProxies, const QString& group, const QString& proxy,
                           const CurrentFormatter& formatCurrent) const;

 private:
  QTreeWidget* m_treeWidget = nullptr;
};

#endif  // PROXYTREEPRESENTER_H
