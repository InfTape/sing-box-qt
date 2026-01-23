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
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onTestAllClicked();
    void onSearchTextChanged(const QString &text);
    void onDelayResult(const ProxyDelayTestResult &result);
    void onTestProgress(int current, int total);
    void onTestCompleted();

private:
    void setupUI();
    void updateProxyItem(QTreeWidgetItem *item, const QJsonObject &proxy);
    QString formatDelay(int delay) const;
    QColor getDelayColor(int delay) const;
    void testSingleNode(const QString &proxy);

    QLineEdit *m_searchEdit;
    QTreeWidget *m_treeWidget;
    QPushButton *m_testAllBtn;
    QPushButton *m_refreshBtn;
    QProgressBar *m_progressBar;
    ProxyService *m_proxyService;
    DelayTestService *m_delayTestService;
    QSet<QString> m_testingNodes;  // Nodes under test.
    QJsonObject m_cachedProxies;   // Cached proxy data.
};

#endif // PROXYVIEW_H
