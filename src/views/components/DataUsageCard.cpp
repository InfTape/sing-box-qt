#include "DataUsageCard.h"
#include <QAbstractItemView>
#include <QHeaderView>
#include <QHostAddress>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonValue>
#include <QLabel>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QVBoxLayout>
#include "utils/home/HomeFormat.h"
#include "views/components/DataUsageBar.h"
#include "widgets/common/SegmentedControl.h"
namespace {
class NoFocusDelegate : public QStyledItemDelegate {
 public:
  explicit NoFocusDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}
  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
    QStyleOptionViewItem opt(option);
    opt.state &= ~QStyle::State_HasFocus;
    QStyledItemDelegate::paint(painter, opt, index);
  }
};
qint64 readLongLong(const QJsonValue& value) {
  if (value.isString()) {
    return value.toString().toLongLong();
  }
  return value.toVariant().toLongLong();
}
QString sanitizeHostLabel(const QString& raw) {
  QString text = raw.trimmed();
  if (text.isEmpty()) return text;
  if (text.startsWith('[')) {
    const int end = text.indexOf(']');
    if (end > 1) {
      return text.mid(1, end - 1);
    }
  }
  if (text.count(':') >= 2) {
    return text;
  }
  const int colonPos = text.lastIndexOf(':');
  if (colonPos > 0 && colonPos + 1 < text.size()) {
    bool ok = false;
    text.mid(colonPos + 1).toInt(&ok);
    if (ok) {
      return text.left(colonPos);
    }
  }
  return text;
}
}  // namespace
DataUsageCard::DataUsageCard(ThemeService* themeService, QWidget* parent) : QFrame(parent) {
  setupUi(themeService);
}
void DataUsageCard::setupUi(ThemeService* themeService) {
  setObjectName("DataUsageCard");
  setFixedHeight(180);
  auto* rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(12, 10, 12, 12);
  rootLayout->setSpacing(6);
  auto* rankingHeader = new QHBoxLayout;
  auto* rankingIcon   = new QLabel;
  rankingIcon->setObjectName("RankingIcon");
  rankingIcon->setText(QString::fromUtf8("\xE2\x89\xA1"));
  auto* rankingTitle = new QLabel(tr("Ranking"));
  rankingTitle->setObjectName("SectionTitle");
  m_rankingModeSelector = new SegmentedControl(this, themeService);
  m_rankingModeSelector->setItems({tr("Proxy"), tr("Process"), tr("Interface"), tr("Hostname")},
                                  {"outbound", "process", "sourceIP", "host"});
  m_rankingModeSelector->setCurrentIndex(3);
  m_clearButton = new QPushButton;
  m_clearButton->setObjectName("DataUsageClearBtn");
  m_clearButton->setFixedSize(20, 20);
  m_clearButton->setCursor(Qt::PointingHandCursor);
  m_clearButton->setToolTip(tr("Clear statistics"));
  rankingHeader->addWidget(rankingIcon);
  rankingHeader->addWidget(rankingTitle);
  rankingHeader->addWidget(m_clearButton);
  rankingHeader->addStretch();
  rankingHeader->addWidget(m_rankingModeSelector);
  rootLayout->addLayout(rankingHeader);
  m_topTable = new QTableWidget;
  m_topTable->setObjectName("DataUsageTopTable");
  m_topTable->setColumnCount(3);
  m_topTable->verticalHeader()->setVisible(false);
  m_topTable->horizontalHeader()->setVisible(false);
  m_topTable->setSelectionMode(QAbstractItemView::NoSelection);
  m_topTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_topTable->setShowGrid(false);
  m_topTable->setSortingEnabled(false);
  m_topTable->verticalHeader()->setDefaultSectionSize(24);
  m_topTable->setItemDelegate(new NoFocusDelegate(m_topTable));
  auto* topHeader = m_topTable->horizontalHeader();
  topHeader->setSectionResizeMode(0, QHeaderView::Stretch);
  topHeader->setSectionResizeMode(1, QHeaderView::Stretch);
  topHeader->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  topHeader->setStretchLastSection(false);
  rootLayout->addWidget(m_topTable);
  m_emptyLabel = new QLabel(tr("No data usage yet"));
  m_emptyLabel->setObjectName("DataUsageEmpty");
  m_emptyLabel->setAlignment(Qt::AlignCenter);
  rootLayout->addWidget(m_emptyLabel);
  m_topTable->hide();
  m_emptyLabel->show();
  connect(m_clearButton, &QPushButton::clicked, this, &DataUsageCard::clearRequested);
  connect(m_rankingModeSelector, &SegmentedControl::currentValueChanged, this, [this]() { refreshTable(); });
}
void DataUsageCard::updateDataUsage(const QJsonObject& snapshot) {
  m_snapshot = snapshot;
  refreshTable();
}
void DataUsageCard::refreshTable() {
  if (!m_topTable || !m_rankingModeSelector || !m_emptyLabel) return;
  const QString     typeKey  = m_rankingModeSelector->currentValue();
  const QJsonObject typeObj  = m_snapshot.value(typeKey).toObject();
  const QJsonArray  entries  = typeObj.value("entries").toArray();
  const int         topLimit = 5;
  const int         topCount = qMin(entries.size(), topLimit);
  if (topCount <= 0) {
    m_topTable->setRowCount(0);
    m_topTable->hide();
    m_emptyLabel->show();
    return;
  }
  m_topTable->setRowCount(topCount);
  qint64 maxTotal = 0;
  for (int i = 0; i < topCount; ++i) {
    const QJsonObject entry = entries.at(i).toObject();
    const qint64      total = readLongLong(entry.value("total"));
    if (total > maxTotal) maxTotal = total;
  }
  if (maxTotal <= 0) maxTotal = 1;
  for (int i = 0; i < topCount; ++i) {
    const QJsonObject entry        = entries.at(i).toObject();
    const QString     rawLabel     = entry.value("label").toString();
    const qint64      upload       = readLongLong(entry.value("upload"));
    const qint64      download     = readLongLong(entry.value("download"));
    const qint64      total        = readLongLong(entry.value("total"));
    const QString     displayLabel = (typeKey == "host") ? formatHostLabel(rawLabel) : rawLabel;
    auto*             nameItem     = new QTableWidgetItem(displayLabel);
    nameItem->setToolTip(rawLabel);
    m_topTable->setItem(i, 0, nameItem);
    auto* bar = new DataUsageBar(m_topTable);
    bar->setLogScaledValue(total, maxTotal);
    bar->setToolTip(tr("Upload: %1\nDownload: %2").arg(formatBytes(upload)).arg(formatBytes(download)));
    m_topTable->setCellWidget(i, 1, bar);
    auto* totalItem = new QTableWidgetItem(formatBytes(total));
    totalItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_topTable->setItem(i, 2, totalItem);
    m_topTable->setRowHeight(i, 20);
  }
  m_emptyLabel->hide();
  m_topTable->show();
}
QString DataUsageCard::formatBytes(qint64 bytes) const {
  return HomeFormat::bytes(bytes);
}
QString DataUsageCard::formatHostLabel(const QString& label) const {
  const QString hostLabel = sanitizeHostLabel(label);
  QHostAddress  addr;
  if (addr.setAddress(hostLabel)) {
    return hostLabel;
  }
  const QStringList parts = hostLabel.split('.');
  if (parts.size() > 2) {
    return parts.at(parts.size() - 2) + "." + parts.at(parts.size() - 1);
  }
  return hostLabel;
}
