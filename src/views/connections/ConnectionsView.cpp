#include "ConnectionsView.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QHeaderView>
#include <QHostAddress>
#include <QIcon>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include "app/interfaces/ThemeService.h"
#include "core/ProxyService.h"
#include "widgets/common/HorizontalScrollBar.h"
#include "widgets/common/RoundedMenu.h"

namespace {
constexpr int SortValueRole    = Qt::UserRole + 1;
constexpr int StartTimeRole    = Qt::UserRole + 2;
constexpr int ArrivalOrderRole = Qt::UserRole + 3;
constexpr int SourceSortRole   = Qt::UserRole + 4;

QString sourceSortKey(const QString& source, int port) {
  const QHostAddress address(source);
  QString           addressKey;
  if (address.protocol() == QAbstractSocket::IPv4Protocol) {
    addressKey = "4|" + QString::number(address.toIPv4Address(), 16)
                            .rightJustified(8, '0');
  } else if (address.protocol() == QAbstractSocket::IPv6Protocol) {
    const Q_IPV6ADDR bytes = address.toIPv6Address();
    addressKey = "6|" + QString::fromLatin1(
                           QByteArray(reinterpret_cast<const char*>(bytes.c), 16)
                               .toHex());
  } else {
    addressKey = "x|" + source;
  }
  // Fixed-width numeric keys preserve IP and port order in string comparisons.
  return addressKey + '|' + QString::number(qMax(0, port)).rightJustified(10, '0') +
         '|' + address.scopeId();
}

class ConnectionTableItem : public QTableWidgetItem {
 public:
  bool operator<(const QTableWidgetItem& other) const override {
    const QVariant left  = data(SortValueRole);
    const QVariant right = other.data(SortValueRole);
    if (left.metaType().id() == QMetaType::QString) {
      const int comparison = QString::compare(
          left.toString(), right.toString(), Qt::CaseInsensitive);
      if (comparison != 0) {
        return comparison < 0;
      }
    } else if (left != right) {
      return left.toLongLong() < right.toLongLong();
    }
    return data(ArrivalOrderRole).toULongLong() <
           other.data(ArrivalOrderRole).toULongLong();
  }
};

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
  QStringList      chains;
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

QString resolveGroupToNode(const QString&                 outbound,
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

QString resolveNodeFromConnection(const QJsonObject&             conn,
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

QString formatRuleText(const QJsonObject&             conn,
                       const QHash<QString, QString>& groupNowMap) {
  const QString rawRule = conn.value("rule").toString().trimmed();
  if (rawRule.isEmpty()) {
    return rawRule;
  }
  const QString routeGroup = extractRouteGroupFromRule(rawRule);
  if (!routeGroup.isEmpty()) {
    const QString node = resolveGroupToNode(routeGroup, groupNowMap);
    if (!node.isEmpty() && node.compare(routeGroup, Qt::CaseInsensitive) != 0) {
      QString                         result = rawRule;
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
      m_autoRefreshEnabled(false),
      m_themeService(themeService) {
  setupUI();
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
  m_closeAllBtn = new QPushButton(tr("Close All"));
  m_closeAllBtn->setObjectName("CloseAllBtn");
  headerLayout->addStretch();
  headerLayout->addWidget(m_closeAllBtn);
  mainLayout->addLayout(headerLayout);
  // Connections table.
  m_tableWidget = new QTableWidget;
  m_tableWidget->setObjectName("ConnectionsTable");
  m_tableWidget->setHorizontalScrollBar(
      new HorizontalScrollBar(m_tableWidget));
  m_tableWidget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_tableWidget->setColumnCount(6);
  m_tableWidget->setHorizontalHeaderLabels({tr("Source"),
                                            tr("Destination"),
                                            tr("Network"),
                                            tr("Rule"),
                                            tr("Upload"),
                                            tr("Download")});
  m_tableWidget->horizontalHeader()->setStretchLastSection(true);
  auto* tableHeader = m_tableWidget->horizontalHeader();
  tableHeader->setSectionsClickable(true);
  tableHeader->setSortIndicatorShown(false);
  connect(tableHeader, &QHeaderView::sectionClicked, this, [this](int column) {
    if (m_sortColumn != column) {
      m_sortColumn = column;
      m_sortOrder  = Qt::AscendingOrder;
    } else if (m_sortOrder == Qt::AscendingOrder) {
      m_sortOrder = Qt::DescendingOrder;
    } else {
      m_sortColumn = -1;
    }
    sortConnections();
  });
  m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_tableWidget, &QTableWidget::customContextMenuRequested,
          this, &ConnectionsView::onContextMenuRequested);
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
            m_closeAllBtn->setText(hasSelection ? tr("Close Selected")
                                                : tr("Close All"));
          });
  updateStyle();
}

void ConnectionsView::onContextMenuRequested(const QPoint& position) {
  const auto* item = m_tableWidget->itemAt(position);
  if (!item) {
    return;
  }
  // The table may refresh or reorder while the menu's event loop is running.
  // Capture the full cell text now, including text elided by the delegate.
  const QString text = item->text();
  RoundedMenu menu(this);
  menu.setObjectName("ComboMenu");
  if (m_themeService) {
    menu.setThemeColors(m_themeService->color("panel-bg"),
                        m_themeService->color("border-solid"));
  }
  // Use the same themed icon and mnemonic as the text editor's standard menu.
  auto* copy = menu.addAction(QIcon::fromTheme("edit-copy"), tr("&Copy"));
  copy->setObjectName("edit-copy");
  copy->setShortcut(QKeySequence::Copy);
  copy->setShortcutVisibleInContextMenu(true);
  connect(copy, &QAction::triggered, &menu, [text]() {
    QApplication::clipboard()->setText(text);
  });
  menu.exec(m_tableWidget->viewport()->mapToGlobal(position));
}

void ConnectionsView::setProxyService(ProxyService* service) {
  if (m_proxyService == service) {
    return;
  }
  if (m_proxyService) {
    disconnect(m_proxyService, nullptr, this, nullptr);
  }
  m_proxyService = service;
  m_connectionOrder.clear();
  m_nextConnectionOrder = 0;
  if (!m_proxyService) {
    return;
  }
  connect(m_proxyService,
          &ProxyService::connectionsReceived,
          this,
          [this](const QJsonObject& connections) {
            if (!m_autoRefreshEnabled) {
              return;
            }
            QJsonArray conns = connections["connections"].toArray();
            const QHash<QString, QString> groupNowMap =
                m_proxyService ? m_proxyService->groupNowCache()
                               : QHash<QString, QString>();
            QSet<QString> selectedIds;
            for (const auto& index :
                 m_tableWidget->selectionModel()->selectedRows()) {
              selectedIds.insert(m_tableWidget->item(index.row(), 0)
                                     ->data(Qt::UserRole)
                                     .toString());
            }
            QString   currentId;
            const int currentRow    = m_tableWidget->currentRow();
            const int currentColumn = m_tableWidget->currentColumn();
            if (currentRow >= 0) {
              currentId = m_tableWidget->item(currentRow, 0)
                              ->data(Qt::UserRole)
                              .toString();
            }
            const QSignalBlocker selectionBlocker(
                m_tableWidget->selectionModel());
            m_tableWidget->clearSelection();
            m_tableWidget->setCurrentItem(nullptr);
            m_tableWidget->setRowCount(conns.count());
            auto setCell = [this](int             row,
                                 int             col,
                                 const QString&  text,
                                 quint64         sequence,
                                 const QVariant& key = {}) {
              QTableWidgetItem* item = m_tableWidget->item(row, col);
              if (!item) {
                item = new ConnectionTableItem();
                m_tableWidget->setItem(row, col, item);
              }
              item->setText(text);
              item->setData(SortValueRole, key.isValid() ? key : text);
              item->setData(ArrivalOrderRole, sequence);
            };
            QSet<QString> activeIds;
            for (int i = 0; i < conns.count(); ++i) {
              QJsonObject   conn     = conns[i].toObject();
              QJsonObject   metadata = conn["metadata"].toObject();
              const QString id       = conn["id"].toString();
              activeIds.insert(id);
              if (!m_connectionOrder.contains(id)) {
                const QDateTime start = QDateTime::fromString(
                    conn["start"].toString(), Qt::ISODateWithMs);
                const qint64 started = start.isValid()
                                           ? start.toMSecsSinceEpoch()
                                           : QDateTime::currentMSecsSinceEpoch();
                m_connectionOrder.insert(id, {started, ++m_nextConnectionOrder});
              }
              const auto  order    = m_connectionOrder.value(id);
              auto        readPort = [](const QJsonValue& value) -> int {
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
              {
                QString    source       = metadata["sourceIP"].toString();
                QJsonValue srcPortValue = metadata.value("sourcePort");
                if (srcPortValue.isUndefined()) {
                  srcPortValue = metadata.value("source_port");
                }
                const int srcPort = readPort(srcPortValue);
                const QString sourceKey = sourceSortKey(source, srcPort);
                if (srcPort > 0) {
                  source += ":" + QString::number(srcPort);
                }
                setCell(i, 0, source, order.second);
                m_tableWidget->item(i, 0)->setData(SourceSortRole, sourceKey);
              }
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
              QJsonValue portValue = metadata.value("destinationPort");
              if (portValue.isUndefined()) {
                portValue = metadata.value("destination_port");
              }
              const int port        = readPort(portValue);
              QString   destination = host;
              if (port > 0) {
                destination += ":" + QString::number(port);
              }
              setCell(i, 1, destination, order.second);
              setCell(i, 2, metadata["network"].toString(), order.second);
              setCell(i, 3, formatRuleText(conn, groupNowMap), order.second);
              const qint64 upload = conn["upload"].toVariant().toLongLong();
              const qint64 download = conn["download"].toVariant().toLongLong();
              setCell(i,
                      4,
                      QString::number(upload / 1024) + " KB",
                      order.second,
                      upload);
              setCell(i,
                      5,
                      QString::number(download / 1024) + " KB",
                      order.second,
                      download);
              // Store connection ID.
              QTableWidgetItem* idItem = m_tableWidget->item(i, 0);
              if (idItem) {
                idItem->setData(Qt::UserRole, conn["id"].toString());
                idItem->setData(StartTimeRole, order.first);
              }
            }
            for (auto it = m_connectionOrder.begin();
                 it != m_connectionOrder.end();) {
              if (!activeIds.contains(it.key())) {
                it = m_connectionOrder.erase(it);
              } else {
                ++it;
              }
            }
            sortConnections();
            auto* selection = m_tableWidget->selectionModel();
            for (int row = 0; row < m_tableWidget->rowCount(); ++row) {
              const QString id = m_tableWidget->item(row, 0)
                                     ->data(Qt::UserRole)
                                     .toString();
              if (!currentId.isEmpty() && id == currentId) {
                selection->setCurrentIndex(
                    m_tableWidget->model()->index(row, currentColumn),
                    QItemSelectionModel::NoUpdate);
              }
              if (selectedIds.contains(id)) {
                selection->select(m_tableWidget->model()->index(row, 0),
                                  QItemSelectionModel::Select |
                                      QItemSelectionModel::Rows);
              }
            }
            m_closeAllBtn->setText(selection->selectedRows().isEmpty()
                                      ? tr("Close All")
                                      : tr("Close Selected"));
          });
}

void ConnectionsView::sortConnections() {
  const bool newestFirst = m_sortColumn < 0;
  for (int row = 0; row < m_tableWidget->rowCount(); ++row) {
    auto* item = m_tableWidget->item(row, 0);
    item->setData(SortValueRole,
                  newestFirst ? item->data(StartTimeRole)
                              : item->data(SourceSortRole));
  }
  m_tableWidget->sortItems(newestFirst ? 0 : m_sortColumn,
                          newestFirst ? Qt::DescendingOrder : m_sortOrder);
  auto* header = m_tableWidget->horizontalHeader();
  header->setSortIndicatorShown(!newestFirst);
}

void ConnectionsView::setAutoRefreshEnabled(bool enabled) {
  m_autoRefreshEnabled = enabled;
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
