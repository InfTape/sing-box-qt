#ifndef CONNECTIONSVIEW_H
#define CONNECTIONSVIEW_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QTimer>

class ProxyService;

class ConnectionsView : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionsView(QWidget *parent = nullptr);
    void setProxyService(ProxyService *service);

private slots:
    void onRefresh();
    void onCloseSelected();
    void onCloseAll();

private:
    void setupUI();
    
    QTableWidget *m_tableWidget;
    QPushButton *m_closeSelectedBtn;
    QPushButton *m_closeAllBtn;
    QTimer *m_refreshTimer;
    ProxyService *m_proxyService;
};

#endif // CONNECTIONSVIEW_H
