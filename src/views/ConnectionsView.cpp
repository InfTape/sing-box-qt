#include "ConnectionsView.h"
#include "core/ProxyService.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>

ConnectionsView::ConnectionsView(QWidget *parent)
    : QWidget(parent)
    , m_proxyService(nullptr)
    , m_refreshTimer(new QTimer(this))
    , m_autoRefreshEnabled(false)
{
    setupUI();
    
    m_refreshTimer->setInterval(1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &ConnectionsView::onRefresh);
}

void ConnectionsView::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);
    
    // 工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout;
    
    m_closeSelectedBtn = new QPushButton(tr("关闭选中"));
    m_closeAllBtn = new QPushButton(tr("关闭全部"));
    
    QString buttonStyle = R"(
        QPushButton {
            background-color: #0f3460;
            color: white;
            border: none;
            padding: 8px 16px;
            border-radius: 10px;
        }
        QPushButton:hover { background-color: #1f4068; }
    )";
    m_closeSelectedBtn->setStyleSheet(buttonStyle);
    m_closeAllBtn->setStyleSheet(buttonStyle.replace("#0f3460", "#ff6b6b").replace("#1f4068", "#ff8585"));
    
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_closeSelectedBtn);
    toolbarLayout->addWidget(m_closeAllBtn);
    
    // 连接表格
    m_tableWidget = new QTableWidget;
    m_tableWidget->setColumnCount(6);
    m_tableWidget->setHorizontalHeaderLabels({
        tr("主机"), tr("目标"), tr("网络"), tr("规则"), tr("上传"), tr("下载")
    });
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setStyleSheet(R"(
        QTableWidget {
            background-color: #16213e;
            border: none;
            border-radius: 10px;
            color: #eaeaea;
            gridline-color: #0f3460;
        }
        QTableWidget::item { padding: 8px; }
        QTableWidget::item:selected { background-color: #0f3460; }
        QHeaderView {
            background: transparent;
        }
        QTableCornerButton::section {
            background-color: #0f3460;
            border-top-left-radius: 10px;
        }
        QHeaderView::section {
            background-color: #0f3460;
            color: #eaeaea;
            padding: 8px;
            border: none;
        }
        QHeaderView::section:first {
            border-top-left-radius: 10px;
        }
        QHeaderView::section:last {
            border-top-right-radius: 10px;
        }
    )");
    
    mainLayout->addLayout(toolbarLayout);
    mainLayout->addWidget(m_tableWidget, 1);
    
    connect(m_closeSelectedBtn, &QPushButton::clicked, this, &ConnectionsView::onCloseSelected);
    connect(m_closeAllBtn, &QPushButton::clicked, this, &ConnectionsView::onCloseAll);
}

void ConnectionsView::setProxyService(ProxyService *service)
{
    m_proxyService = service;
    
    if (m_proxyService) {
        connect(m_proxyService, &ProxyService::connectionsReceived, this,
                [this](const QJsonObject &connections) {
            QJsonArray conns = connections["connections"].toArray();
            m_tableWidget->setRowCount(conns.count());

            auto setCell = [this](int row, int col, const QString &text) {
                QTableWidgetItem *item = m_tableWidget->item(row, col);
                if (!item) {
                    item = new QTableWidgetItem();
                    m_tableWidget->setItem(row, col, item);
                }
                item->setText(text);
            };

            for (int i = 0; i < conns.count(); ++i) {
                QJsonObject conn = conns[i].toObject();
                QJsonObject metadata = conn["metadata"].toObject();

                setCell(i, 0, metadata["sourceIP"].toString());
                setCell(i, 1, metadata["host"].toString() + ":" + QString::number(metadata["destinationPort"].toInt()));
                setCell(i, 2, metadata["network"].toString());
                setCell(i, 3, conn["rule"].toString());
                setCell(i, 4, QString::number(conn["upload"].toVariant().toLongLong() / 1024) + " KB");
                setCell(i, 5, QString::number(conn["download"].toVariant().toLongLong() / 1024) + " KB");

                // 保存连接 ID
                QTableWidgetItem *idItem = m_tableWidget->item(i, 0);
                if (idItem) {
                    idItem->setData(Qt::UserRole, conn["id"].toString());
                }
            }
        });
    }
}

void ConnectionsView::setAutoRefreshEnabled(bool enabled)
{
    m_autoRefreshEnabled = enabled;
    if (m_autoRefreshEnabled && m_proxyService) {
        if (!m_refreshTimer->isActive()) {
            m_refreshTimer->start();
        }
        onRefresh();
    } else {
        m_refreshTimer->stop();
    }
}

void ConnectionsView::onRefresh()
{
    if (m_proxyService && m_autoRefreshEnabled) {
        m_proxyService->fetchConnections();
    }
}

void ConnectionsView::onCloseSelected()
{
    if (!m_proxyService) return;
    
    QList<QTableWidgetItem*> selected = m_tableWidget->selectedItems();
    QSet<int> rows;
    for (auto item : selected) {
        rows.insert(item->row());
    }
    
    for (int row : rows) {
        QString id = m_tableWidget->item(row, 0)->data(Qt::UserRole).toString();
        m_proxyService->closeConnection(id);
    }
}

void ConnectionsView::onCloseAll()
{
    if (m_proxyService) {
        m_proxyService->closeAllConnections();
    }
}
