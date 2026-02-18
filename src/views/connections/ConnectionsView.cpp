#include "ConnectionsView.h"
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QRegularExpression>
#include <QSet>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include "app/interfaces/ThemeService.h"
#include "core/ProxyService.h"

namespace {
class ConnectionsItemDelegate : public QStyledItemDelegate {
 public:
  explicit ConnectionsItemDelegate(QObject* parent = nullptr)
      : QStyledItemDelegate(parent) {}

  void paint(QPainter*                   painter,
             const QStyleOptionViewItem& option,
             const QModelIndex&          index) const override {
    QStyleOptionViewItem opt(option);
    opt.state &= ~QStyle::State_HasFocus;
    opt.showDecorationSelected = true;
    QStyledItemDelegate::paint(painter, opt, index);
  }
};

QStringList extractConnectionChains(const QJsonObject& conn) {
  QStringList chains;
  const QJsonArray chainArray = conn.value("chains").toArray();
  for (const auto& chainVal : chainArray) {
    const QString chain = chainVal.toString().trimmed();
    if (!chain.isEmpty()) {
      chains.append(chain);
    }
  }
  if (chains.isEmpty()) {
    const QString outbound = conn.value("outbound").toString().trimmed();
    if (!outbound.isEmpty()) {
      chains.append(outbound);
    }
  }
  return chains;
}

QString resolveGroupToNode(const QString& outbound,
                           const QHash<QString, QString>& groupNowMap) {
  QString current = outbound.trimmed();
  if (current.isEmpty()) {
    return current;
  }
  QSet<QString> visited;
  while (!current.isEmpty() && groupNowMap.contains(current)) {
    if (visited.contains(current)) {
      break;
    }
    visited.insert(current);
    const QString next = groupNowMap.value(current).trimmed();
    if (next.isEmpty() || next.compare(current, Qt::CaseInsensitive) == 0) {
      break;
    }
    current = next;
  }
  return current;
}

QString resolveNodeFromConnection(const QJsonObject&              conn,
                                  const QHash<QString, QString>& groupNowMap) {
  const QStringList chains = extractConnectionChains(conn);
  if (!chains.isEmpty()) {
    for (int i = chains.size() - 1; i >= 0; --i) {
      const QString candidate = chains[i].trimmed();
      if (candidate.isEmpty()) {
        continue;
      }
      const QString resolved = resolveGroupToNode(candidate, groupNowMap);
      if (!resolved.isEmpty() &&
          (resolved.compare(candidate, Qt::CaseInsensitive) != 0 ||
           !groupNowMap.contains(resolved))) {
        return resolved;
      }
    }
    return resolveGroupToNode(chains.last(), groupNowMap);
  }
  QString outbound = conn.value("outbound").toString().trimmed();
  if (outbound.isEmpty()) {
    const QJsonObject meta = conn.value("metadata").toObject();
    outbound               = meta.value("outbound").toString().trimmed();
  }
  return resolveGroupToNode(outbound, groupNowMap);
}

QString extractRouteGroupFromRule(const QString& rawRule) {
  static const QRegularExpression routeExpr(R"(route\(([^)]+)\))");
  const QRegularExpressionMatch   match = routeExpr.match(rawRule.trimmed());
  if (!match.hasMatch()) {
    return QString();
  }
  return match.captured(1).trimmed();
}

QString formatRuleText(const QJsonObject& conn,
                       const QHash<QString, QString>& groupNowMap) {
  const QString rawRule = conn.value("rule").toString().trimmed();
  if (rawRule.isEmpty()) {
    return rawRule;
  }
  const QString routeGroup = extractRouteGroupFromRule(rawRule);
  if (!routeGroup.isEmpty()) {
    const QString node       = resolveGroupToNode(routeGroup, groupNowMap);
    if (!node.isEmpty() &&
        node.compare(routeGroup, Qt::CaseInsensitive) != 0) {
      QString result = rawRule;
      static const QRegularExpression routeExpr(R"(route\(([^)]+)\))");
      const QRegularExpressionMatch   match = routeExpr.match(rawRule);
      result.replace(match.capturedStart(0),
                     match.capturedLength(0),
                     QStringLiteral("route(%1 => %2)").arg(routeGroup, node));
      return result;
    }
    return rawRule;
  }
  const QString node = resolveNodeFromConnection(conn, groupNowMap);
  if (node.isEmpty() || node.compare(rawRule, Qt::CaseInsensitive) == 0) {
    return rawRule;
  }
  if (rawRule.contains("=>")) {
    QStringList parts = rawRule.split("=>");
    if (parts.size() >= 2) {
      const QString tail = parts.last().trimmed();
      if (!tail.isEmpty() && !tail.contains('(') && !tail.contains(')')) {
        if (tail.compare(node, Qt::CaseInsensitive) == 0) {
          return rawRule;
        }
        parts[parts.size() - 1] = node;
        for (auto& part : parts) {
          part = part.trimmed();
        }
        return parts.join(QStringLiteral(" => "));
      }
    }
  }
  return QStringLiteral("%1 => %2").arg(rawRule, node);
}
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
    connect(m_themeService,
            &ThemeService::themeChanged,
            this,
            &ConnectionsView::updateStyle);
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
  m_closeAllBtn      = new QPushButton(tr("Close All"));
  m_closeAllBtn->setObjectName("CloseAllBtn");
  headerLayout->addStretch();
  headerLayout->addWidget(m_closeAllBtn);
  mainLayout->addLayout(headerLayout);
  // Connections table.
  m_tableWidget = new QTableWidget;
  m_tableWidget->setObjectName("ConnectionsTable");
  m_tableWidget->setColumnCount(6);
  m_tableWidget->setHorizontalHeaderLabels({tr("Source"),
                                            tr("Destination"),
                                            tr("Network"),
                                            tr("Rule"),
                                            tr("Upload"),
                                            tr("Download")});
  m_tableWidget->horizontalHeader()->setStretchLastSection(true);
  m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_tableWidget->setItemDelegate(new ConnectionsItemDelegate(m_tableWidget));
  mainLayout->addWidget(m_tableWidget, 1);
  connect(
      m_closeAllBtn, &QPushButton::clicked, this, &ConnectionsView::onCloseAll);
  connect(m_tableWidget->selectionModel(),
          &QItemSelectionModel::selectionChanged,
          this,
          [this]() {
            const bool hasSelection =
                !m_tableWidget->selectionModel()->selectedRows().isEmpty();
            m_closeAllBtn->setText(
                hasSelection ? tr("Close Selected") : tr("Close All"));
          });
  updateStyle();
}

void ConnectionsView::setProxyService(ProxyService* service) {
  if (m_proxyService == service) {
    return;
  }
  if (m_proxyService) {
    disconnect(m_proxyService, nullptr, this, nullptr);
  }
  m_proxyService = service;
  if (!m_proxyService) {
    return;
  }
  connect(m_proxyService,
          &ProxyService::connectionsReceived,
          this,
          [this](const QJsonObject& connections) {
            QJsonArray conns = connections["connections"].toArray();
            const QHash<QString, QString> groupNowMap =
                m_proxyService ? m_proxyService->groupNowCache()
                               : QHash<QString, QString>();
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
              if (host.isEmpty()) {
                host = metadata["destinationIP"].toString();
              }
              if (host.isEmpty()) {
                host = metadata["destinationIp"].toString();
              }
              if (host.isEmpty()) {
                host = tr("Unknown");
              }
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
              setCell(i, 3, formatRuleText(conn, groupNowMap));
              setCell(i,
                      4,
                      QString::number(
                          conn["upload"].toVariant().toLongLong() / 1024) +
                          " KB");
              setCell(i,
                      5,
                      QString::number(
                          conn["download"].toVariant().toLongLong() / 1024) +
                          " KB");
              // Store connection ID.
              QTableWidgetItem* idItem = m_tableWidget->item(i, 0);
              if (idItem) {
                idItem->setData(Qt::UserRole, conn["id"].toString());
              }
            }
          });
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

void ConnectionsView::onCloseAll() {
  if (!m_proxyService) {
    return;
  }
  const auto selectedRows = m_tableWidget->selectionModel()->selectedRows();
  if (!selectedRows.isEmpty()) {
    for (const auto& idx : selectedRows) {
      QString id =
          m_tableWidget->item(idx.row(), 0)->data(Qt::UserRole).toString();
      m_proxyService->closeConnection(id);
    }
  } else {
    m_proxyService->closeAllConnections();
  }
}

void ConnectionsView::updateStyle() {
  ThemeService* ts = m_themeService;
  if (ts) {
    setStyleSheet(ts->loadStyleSheet(":/styles/connections_view.qss"));
  }
}
