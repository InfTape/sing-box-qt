#include "ConnectionsView.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include "app/interfaces/ThemeService.h"
#include "core/ProxyService.h"

namespace {
class ConnectionsItemDelegate : public QStyledItemDelegate {
 public:
  explicit ConnectionsItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
    QStyleOptionViewItem opt(option);
    opt.state &= ~QStyle::State_HasFocus;
    opt.showDecorationSelected = true;
    QStyledItemDelegate::paint(painter, opt, index);
  }
};
}  // namespace

ConnectionsView::ConnectionsView(ThemeService* themeService, QWidget* parent)
    : QWidget(parent),
      m_proxyService(nullptr),
      m_refreshTimer(new QTimer(this)),
      m_autoRefreshEnabled(false),
      m_themeService(themeService) {
  setupUI();
  m_refreshTimer->setInterval(1000);
  connect(m_refreshTimer, &QTimer::timeout, this, &ConnectionsView::onRefresh);
  if (m_themeService) {
    connect(m_themeService, &ThemeService::themeChanged, this, &ConnectionsView::updateStyle);
  }
}

void ConnectionsView::setupUI() {
  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(24, 24, 24, 24);
  mainLayout->setSpacing(12);
  // Header (align with Rules page style)
  QHBoxLayout* headerLayout = new QHBoxLayout;
  QVBoxLayout* titleLayout  = new QVBoxLayout;
  titleLayout->setSpacing(4);
  QLabel* titleLabel = new QLabel(tr("Connections"));
  titleLabel->setObjectName("PageTitle");
  QLabel* subtitleLabel = new QLabel(tr("View and manage active connections"));
  subtitleLabel->setObjectName("PageSubtitle");
  titleLayout->addWidget(titleLabel);
  titleLayout->addWidget(subtitleLabel);
  headerLayout->addLayout(titleLayout);
  m_closeSelectedBtn = new QPushButton(tr("Close Selected"));
  m_closeAllBtn      = new QPushButton(tr("Close All"));
  m_closeSelectedBtn->setObjectName("CloseSelectedBtn");
  m_closeAllBtn->setObjectName("CloseAllBtn");
  headerLayout->addStretch();
  headerLayout->addWidget(m_closeSelectedBtn);
  headerLayout->addWidget(m_closeAllBtn);
  mainLayout->addLayout(headerLayout);
  // Connections table.
  m_tableWidget = new QTableWidget;
  m_tableWidget->setObjectName("ConnectionsTable");
  m_tableWidget->setColumnCount(6);
  m_tableWidget->setHorizontalHeaderLabels(
      {tr("Source"), tr("Destination"), tr("Network"), tr("Rule"), tr("Upload"), tr("Download")});
  m_tableWidget->horizontalHeader()->setStretchLastSection(true);
  m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_tableWidget->setItemDelegate(new ConnectionsItemDelegate(m_tableWidget));
  mainLayout->addWidget(m_tableWidget, 1);
  connect(m_closeSelectedBtn, &QPushButton::clicked, this, &ConnectionsView::onCloseSelected);
  connect(m_closeAllBtn, &QPushButton::clicked, this, &ConnectionsView::onCloseAll);
  updateStyle();
}

void ConnectionsView::setProxyService(ProxyService* service) {
  m_proxyService = service;
  if (m_proxyService) {
    connect(m_proxyService, &ProxyService::connectionsReceived, this, [this](const QJsonObject& connections) {
      QJsonArray conns = connections["connections"].toArray();
      m_tableWidget->setRowCount(conns.count());
      auto setCell = [this](int row, int col, const QString& text) {
        QTableWidgetItem* item = m_tableWidget->item(row, col);
        if (!item) {
          item = new QTableWidgetItem();
          m_tableWidget->setItem(row, col, item);
        }
        item->setText(text);
      };
      for (int i = 0; i < conns.count(); ++i) {
        QJsonObject conn     = conns[i].toObject();
        QJsonObject metadata = conn["metadata"].toObject();
        setCell(i, 0, metadata["sourceIP"].toString());
        QString host = metadata["host"].toString();
        if (host.isEmpty()) host = metadata["destinationIP"].toString();
        if (host.isEmpty()) host = metadata["destinationIp"].toString();
        if (host.isEmpty()) host = tr("Unknown");
        auto readPort = [](const QJsonValue& value) -> int {
          if (value.isString()) {
            bool      ok     = false;
            const int parsed = value.toString().toInt(&ok);
            return ok ? parsed : 0;
          }
          if (value.isDouble()) {
            return value.toInt();
          }
          bool      ok     = false;
          const int parsed = value.toVariant().toInt(&ok);
          return ok ? parsed : 0;
        };
        QJsonValue portValue = metadata.value("destinationPort");
        if (portValue.isUndefined()) {
          portValue = metadata.value("destination_port");
        }
        const int port        = readPort(portValue);
        QString   destination = host;
        if (port > 0) {
          destination += ":" + QString::number(port);
        }
        setCell(i, 1, destination);
        setCell(i, 2, metadata["network"].toString());
        setCell(i, 3, conn["rule"].toString());
        setCell(i, 4, QString::number(conn["upload"].toVariant().toLongLong() / 1024) + " KB");
        setCell(i, 5, QString::number(conn["download"].toVariant().toLongLong() / 1024) + " KB");
        // Store connection ID.
        QTableWidgetItem* idItem = m_tableWidget->item(i, 0);
        if (idItem) {
          idItem->setData(Qt::UserRole, conn["id"].toString());
        }
      }
    });
  }
}

void ConnectionsView::setAutoRefreshEnabled(bool enabled) {
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

void ConnectionsView::onRefresh() {
  if (m_proxyService && m_autoRefreshEnabled) {
    m_proxyService->fetchConnections();
  }
}

void ConnectionsView::onCloseSelected() {
  if (!m_proxyService) return;
  QList<QTableWidgetItem*> selected = m_tableWidget->selectedItems();
  QSet<int>                rows;
  for (auto item : selected) {
    rows.insert(item->row());
  }
  for (int row : rows) {
    QString id = m_tableWidget->item(row, 0)->data(Qt::UserRole).toString();
    m_proxyService->closeConnection(id);
  }
}

void ConnectionsView::onCloseAll() {
  if (m_proxyService) {
    m_proxyService->closeAllConnections();
  }
}

void ConnectionsView::updateStyle() {
  ThemeService* ts = m_themeService;
  if (ts) setStyleSheet(ts->loadStyleSheet(":/styles/connections_view.qss"));
}
