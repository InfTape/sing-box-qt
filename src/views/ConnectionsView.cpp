#include "ConnectionsView.h"
#include "core/ProxyService.h"
#include "utils/ThemeManager.h"
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
    
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &ConnectionsView::updateStyle);
}

void ConnectionsView::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // Toolbar.
    QHBoxLayout *toolbarLayout = new QHBoxLayout;

    m_closeSelectedBtn = new QPushButton(tr("Close Selected"));
    m_closeAllBtn = new QPushButton(tr("Close All"));

    m_closeSelectedBtn = new QPushButton(tr("Close Selected"));
    m_closeAllBtn = new QPushButton(tr("Close All"));

    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_closeSelectedBtn);
    toolbarLayout->addWidget(m_closeAllBtn);

    // Connections table.
    m_tableWidget = new QTableWidget;
    m_tableWidget->setColumnCount(6);
    m_tableWidget->setHorizontalHeaderLabels({
        tr("Source"), tr("Destination"), tr("Network"), tr("Rule"), tr("Upload"), tr("Download")
    });
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setStyleSheet(R"(
        QTableWidget {
            background-color: transparent;
            border: none;
            border-radius: 10px;
            color: #eaeaea;
            gridline-color: #0f3460;
        }
        QTableWidget::item { padding: 8px; }
        QTableWidget::item:selected { background-color: rgba(62, 166, 255, 0.2); }
        QHeaderView {
            background: transparent;
        }
        QTableCornerButton::section {
            background-color: rgba(62, 166, 255, 0.2);
            border-top-left-radius: 10px;
        }
        QHeaderView::section {
            background-color: rgba(62, 166, 255, 0.15);
            color: #eaeaea;
            padding: 8px;
            border: none;
        }
        QHeaderView::section:first {
            /* border-top-left-radius removed to align with corner button */
        }
        QHeaderView::section:last {
            border-top-right-radius: 10px;
        }
    )");

    mainLayout->addLayout(toolbarLayout);
    mainLayout->addWidget(m_tableWidget, 1);

    connect(m_closeSelectedBtn, &QPushButton::clicked, this, &ConnectionsView::onCloseSelected);
    connect(m_closeAllBtn, &QPushButton::clicked, this, &ConnectionsView::onCloseAll);
    
    updateStyle();
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

                // Store connection ID.
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

void ConnectionsView::updateStyle()
{
    ThemeManager &tm = ThemeManager::instance();

    auto applyTransparentStyle = [](QPushButton *btn, const QColor &baseColor) {
        if (!btn) return;
        QColor bg = baseColor; bg.setAlphaF(0.2);
        QColor border = baseColor; border.setAlphaF(0.4);
        QColor hover = baseColor; hover.setAlphaF(0.3);
        
        btn->setStyleSheet(QString(
            "QPushButton { background-color: %1; color: %2; border: 1px solid %3; "
            "border-radius: 10px; padding: 8px 16px; font-weight: bold; }"
            "QPushButton:hover { background-color: %4; }"
        ).arg(bg.name(QColor::HexArgb), baseColor.name(), border.name(QColor::HexArgb), hover.name(QColor::HexArgb)));
    };

    applyTransparentStyle(m_closeSelectedBtn, QColor("#3b82f6"));
    applyTransparentStyle(m_closeAllBtn, QColor("#e94560"));
}
