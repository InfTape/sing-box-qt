#ifndef PROXYVIEW_H
#define PROXYVIEW_H

#include <QHash>
#include <QJsonObject>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QTreeWidget>
#include <QWidget>

class ThemeService;
struct ProxyDelayTestResult;
class ProxyViewController;
class ProxyView : public QWidget {
  Q_OBJECT

 public:
  explicit ProxyView(ThemeService* themeService, QWidget* parent = nullptr);

  void setController(ProxyViewController* controller);
  void refresh();

 private slots:
  void updateStyle();
  void onProxiesReceived(const QJsonObject& proxies);
  void onItemClicked(QTreeWidgetItem* item, int column);
  void onItemDoubleClicked(QTreeWidgetItem* item, int column);
  void onProxySelected(const QString& group, const QString& proxy);
  void onProxySelectFailed(const QString& group, const QString& proxy);
  void onTestSelectedClicked();
  void onTestAllClicked();
  void onSearchTextChanged(const QString& text);
  void onDelayResult(const ProxyDelayTestResult& result);
  void onTestProgress(int current, int total);
  void onTestCompleted();
  void onTreeContextMenu(const QPoint& pos);
  void startSpeedTest(QTreeWidgetItem* item);
  void onSpeedTestResult(const QString& nodeName, const QString& result);

 private:
  void     setupUI();
  void     handleNodeActivation(QTreeWidgetItem* item);
  void     renderProxies(const QJsonObject& proxies);
  void     updateSelectedProxyUI(const QString& group, const QString& proxy);
  QString  formatDelay(int delay) const;
  QWidget* buildNodeRow(const QString& name, const QString& type,
                        const QString& delay) const;
  void     updateNodeRowDelay(QTreeWidgetItem* item, const QString& delayText,
                              const QString& state);
  QString  nodeDisplayName(QTreeWidgetItem* item) const;
  void     updateNodeRowSelected(QTreeWidgetItem* item, bool selected);
  void     onSelectionChanged(const QItemSelection& selected,
                              const QItemSelection& deselected);
  void     applyTreeItemColors();
  void     markNodeState(QTreeWidgetItem* item, const QString& group,
                         const QString& now, const QString& delayText = QString());
  void     updateTestButtonStyle(bool testing);
  QJsonObject loadNodeOutbound(const QString& tag) const;
  bool        isTesting() const;

  QLineEdit*              m_searchEdit;
  QTreeWidget*            m_treeWidget;
  QPushButton*            m_testAllBtn;
  QPushButton*            m_testSelectedBtn;
  QPushButton*            m_refreshBtn;
  QProgressBar*           m_progressBar;
  QSet<QString>           m_testingNodes;      // Nodes under test.
  QJsonObject             m_cachedProxies;     // Cached proxy data.
  QHash<QString, QString> m_pendingSelection;  // group -> proxy
  bool                    m_singleTesting{false};
  QString                 m_singleTestingTarget;
  ThemeService*           m_themeService = nullptr;
  ProxyViewController*    m_controller   = nullptr;
};
#endif  // PROXYVIEW_H
