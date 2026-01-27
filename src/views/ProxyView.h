#ifndef PROXYVIEW_H
#define PROXYVIEW_H

#include <QWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QProgressBar>
#include <QHash>

class ProxyService;
class DelayTestService;
struct ProxyDelayTestResult;

class ProxyView : public QWidget
{
    Q_OBJECT

public:
    explicit ProxyView(QWidget *parent = nullptr);

    void setProxyService(ProxyService *service);
    void refresh();

private slots:
    void updateStyle();
    void onProxiesReceived(const QJsonObject &proxies);
    void onItemClicked(QTreeWidgetItem *item, int column);
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onProxySelected(const QString &group, const QString &proxy);
    void onProxySelectFailed(const QString &group, const QString &proxy);
    void onTestSelectedClicked();
    void onTestAllClicked();
    void onSearchTextChanged(const QString &text);
    void onDelayResult(const ProxyDelayTestResult &result);
    void onTestProgress(int current, int total);
    void onTestCompleted();

private:
    void setupUI();
    void handleNodeActivation(QTreeWidgetItem *item);
    void renderProxies(const QJsonObject &proxies);
    void updateSelectedProxyUI(const QString &group, const QString &proxy);
    void updateProxyItem(QTreeWidgetItem *item, const QJsonObject &proxy);
    QString formatDelay(int delay) const;
    void testSingleNode(const QString &proxy);
    QWidget* buildNodeRow(const QString &name, const QString &type, const QString &delay) const;
    void updateNodeRowDelay(QTreeWidgetItem *item, const QString &delayText, const QString &state);
    QString nodeDisplayName(QTreeWidgetItem *item) const;
    void updateNodeRowSelected(QTreeWidgetItem *item, bool selected);
    void onSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void applyTreeItemColors();
    void markNodeState(QTreeWidgetItem *item, const QString &group, const QString &now, const QString &delayText = QString());
    void updateTestButtonStyle(bool testing);

    QLineEdit *m_searchEdit;
    QTreeWidget *m_treeWidget;
    QPushButton *m_testAllBtn;
    QPushButton *m_testSelectedBtn;
    QPushButton *m_refreshBtn;
    QProgressBar *m_progressBar;
    ProxyService *m_proxyService;
    DelayTestService *m_delayTestService;
    QSet<QString> m_testingNodes;  // Nodes under test.
    QJsonObject m_cachedProxies;   // Cached proxy data.
    QHash<QString, QString> m_pendingSelection; // group -> proxy
    bool m_singleTesting{false};
    QString m_singleTestingTarget;
};

#endif // PROXYVIEW_H
