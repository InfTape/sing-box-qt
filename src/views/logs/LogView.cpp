#include "LogView.h"
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QShowEvent>
#include <QMessageBox>
#include <QFutureWatcher>
#include <QLabel>
#include "widgets/common/StyledLineEdit.h"
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>
#include "app/interfaces/ThemeService.h"
#include "utils/LogParser.h"
#include "utils/LogRetention.h"
#include "widgets/common/MenuComboBox.h"
#include "widgets/logs/LogRowWidget.h"

LogView::LogView(ThemeService* themeService, QWidget* parent)
    : QWidget(parent), m_themeService(themeService) {
  m_store = new LogStore(this);
  m_tailScrollTimer = new QTimer(this);
  m_tailScrollTimer->setSingleShot(true);
  m_tailScrollTimer->setInterval(0);
  connect(m_tailScrollTimer, &QTimer::timeout, this, [this]() {
    auto* bar = m_scrollArea->verticalScrollBar();
    if (m_followTail && !bar->isSliderDown()) {
      bar->setValue(bar->maximum());
    }
  });
  setupUI();
  m_refreshTimer = new QTimer(this);
  m_refreshTimer->setInterval(500);
  m_refreshTimer->setSingleShot(true);
  connect(m_refreshTimer, &QTimer::timeout, this, [this]() {
    if (isVisible()) {
      rebuildList();
    }
  });
  connect(m_store, &LogStore::changed, this, &LogView::scheduleRefresh);
  connect(m_store, &LogStore::storageError, this, [this](const QString& message) {
    m_storageError->setText(tr("Log storage error: %1").arg(message));
    m_storageError->show();
  });
  if (!m_store->error().isEmpty()) {
    m_storageError->setText(tr("Log storage error: %1").arg(m_store->error()));
    m_storageError->show();
  }
  updateStyle();
  if (m_themeService) {
    connect(m_themeService, &ThemeService::themeChanged,
            this, &LogView::updateStyle);
  }
}

void LogView::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  rebuildList();
}

void LogView::scheduleRefresh() {
  if (isVisible() && !m_refreshTimer->isActive()) {
    m_refreshTimer->start();
  }
}

void LogView::scheduleTailScroll() {
  if (m_followTail && !m_scrollArea->verticalScrollBar()->isSliderDown() &&
      !m_tailScrollTimer->isActive()) {
    m_tailScrollTimer->start();
  }
}

void LogView::updateScrollIntent() {
  auto* bar = m_scrollArea->verticalScrollBar();
  // actionTriggered fires before value is committed, so use sliderPosition.
  // Only user scrolling changes follow mode; layout changes must not pause it.
  m_followTail = bar->sliderPosition() == bar->maximum();
  if (m_followTail) {
    scheduleRefresh();
    scheduleTailScroll();
  } else {
    m_tailScrollTimer->stop();
  }
}

void LogView::setupUI() {
  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(24, 24, 24, 24);
  mainLayout->setSpacing(16);
  // Header
  QHBoxLayout* headerLayout = new QHBoxLayout;
  QVBoxLayout* titleLayout  = new QVBoxLayout;
  titleLayout->setSpacing(4);
  m_titleLabel = new QLabel(tr("Logs"));
  m_titleLabel->setObjectName("PageTitle");
  m_subtitleLabel = new QLabel(tr("Kernel logs from the last 24 hours"));
  m_subtitleLabel->setObjectName("PageSubtitle");
  titleLayout->addWidget(m_titleLabel);
  titleLayout->addWidget(m_subtitleLabel);
  QWidget*     controls       = new QWidget;
  QHBoxLayout* controlsLayout = new QHBoxLayout(controls);
  controlsLayout->setContentsMargins(0, 0, 0, 0);
  controlsLayout->setSpacing(8);
  m_totalTag = new QLabel(tr("0 entries"));
  m_totalTag->setObjectName("TotalTag");
  m_totalTag->setFixedHeight(32);
  m_errorTag = new QLabel(tr("Errors: 0"));
  m_errorTag->setObjectName("ErrorTag");
  m_errorTag->setFixedHeight(32);
  m_warningTag = new QLabel(tr("Warnings: 0"));
  m_warningTag->setObjectName("WarningTag");
  m_warningTag->setFixedHeight(32);
  m_clearBtn = new QPushButton(tr("Clear"));
  m_clearBtn->setObjectName("ClearBtn");
  m_clearBtn->setCursor(Qt::PointingHandCursor);
  m_clearBtn->setFixedHeight(32);
  m_copyBtn = new QPushButton(tr("Copy"));
  m_copyBtn->setObjectName("CopyBtn");
  m_copyBtn->setToolTip(tr("Copy the displayed logs (up to %1 entries)")
                            .arg(LogStore::kVisibleLimit));
  m_copyBtn->setCursor(Qt::PointingHandCursor);
  m_copyBtn->setFixedHeight(32);
  m_exportBtn = new QPushButton(tr("Export"));
  m_exportBtn->setObjectName("ExportBtn");
  m_exportBtn->setToolTip(tr("Export all matching logs from the last 24 hours"));
  m_exportBtn->setCursor(Qt::PointingHandCursor);
  m_exportBtn->setFixedHeight(32);
  controlsLayout->addWidget(m_totalTag);
  controlsLayout->addWidget(m_errorTag);
  controlsLayout->addWidget(m_warningTag);
  controlsLayout->addSpacing(6);
  controlsLayout->addWidget(m_clearBtn);
  controlsLayout->addWidget(m_copyBtn);
  controlsLayout->addWidget(m_exportBtn);
  headerLayout->addLayout(titleLayout);
  headerLayout->addStretch();
  headerLayout->addWidget(controls);
  mainLayout->addLayout(headerLayout);
  // Filters
  QFrame* filterCard = new QFrame;
  filterCard->setObjectName("FilterCard");
  QHBoxLayout* filterLayout = new QHBoxLayout(filterCard);
  filterLayout->setContentsMargins(14, 12, 14, 12);
  filterLayout->setSpacing(12);
  m_searchEdit = new StyledLineEdit;
  m_searchEdit->setObjectName("SearchInput");
  m_searchEdit->setPlaceholderText(tr("Search logs..."));
  m_searchEdit->setClearButtonEnabled(true);
  m_typeFilter = new MenuComboBox(this, m_themeService);
  m_typeFilter->setObjectName("FilterSelect");
  m_typeFilter->addItem(tr("Type"), QString());
  m_typeFilter->addItem("TRACE", "trace");
  m_typeFilter->addItem("DEBUG", "debug");
  m_typeFilter->addItem("INFO", "info");
  m_typeFilter->addItem("WARN", "warning");
  m_typeFilter->addItem("ERROR", "error");
  m_typeFilter->addItem("FATAL", "fatal");
  m_typeFilter->addItem("PANIC", "panic");
  filterLayout->addWidget(m_searchEdit, 2);
  filterLayout->addWidget(m_typeFilter, 1);
  mainLayout->addWidget(filterCard);
  // Logs list
  QFrame* logCard = new QFrame;
  logCard->setObjectName("LogCard");
  QVBoxLayout* logCardLayout = new QVBoxLayout(logCard);
  logCardLayout->setContentsMargins(0, 0, 0, 0);
  logCardLayout->setSpacing(0);
  m_scrollArea = new QScrollArea;
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setFrameShape(QFrame::NoFrame);
  m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_listContainer = new QWidget;
  m_listLayout    = new QVBoxLayout(m_listContainer);
  m_listLayout->setContentsMargins(12, 12, 12, 12);
  m_listLayout->setSpacing(6);
  m_listLayout->addStretch();
  m_scrollArea->setWidget(m_listContainer);
  m_emptyState = new QFrame;
  m_emptyState->setObjectName("EmptyState");
  m_emptyState->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  QVBoxLayout* emptyLayout = new QVBoxLayout(m_emptyState);
  emptyLayout->setContentsMargins(0, 0, 0, 0);
  emptyLayout->setSpacing(10);
  emptyLayout->setAlignment(Qt::AlignCenter);
  QLabel* emptyIcon = new QLabel(tr("!"));
  emptyIcon->setObjectName("EmptyIcon");
  emptyIcon->setAlignment(Qt::AlignCenter);
  m_emptyTitle = new QLabel(tr("No logs yet"));
  m_emptyTitle->setObjectName("EmptyTitle");
  m_emptyTitle->setAlignment(Qt::AlignCenter);
  emptyLayout->addWidget(emptyIcon);
  emptyLayout->addWidget(m_emptyTitle);
  logCardLayout->addWidget(m_scrollArea, 1);
  logCardLayout->addWidget(m_emptyState, 1);
  mainLayout->addWidget(logCard, 1);
  m_storageError = new QLabel;
  m_storageError->setObjectName("StorageError");
  m_storageError->setWordWrap(true);
  m_storageError->hide();
  mainLayout->addWidget(m_storageError);
  auto* scrollBar = m_scrollArea->verticalScrollBar();
  connect(scrollBar, &QScrollBar::actionTriggered,
          this, &LogView::updateScrollIntent);
  connect(scrollBar, &QScrollBar::sliderMoved,
          this, &LogView::updateScrollIntent);
  connect(scrollBar, &QScrollBar::sliderPressed, this, [this]() {
    m_followTail = false;
    m_tailScrollTimer->stop();
  });
  connect(scrollBar, &QScrollBar::sliderReleased,
          this, &LogView::updateScrollIntent);
  connect(scrollBar, &QScrollBar::rangeChanged,
          this, &LogView::scheduleTailScroll);
  connect(
      m_searchEdit, &QLineEdit::textChanged, this, &LogView::onFilterChanged);
  connect(m_typeFilter,
          QOverload<int>::of(&QComboBox::currentIndexChanged),
          this,
          &LogView::onFilterChanged);
  connect(m_clearBtn, &QPushButton::clicked, this, &LogView::onClearClicked);
  connect(m_copyBtn, &QPushButton::clicked, this, &LogView::onCopyClicked);
  connect(m_exportBtn, &QPushButton::clicked, this, &LogView::onExportClicked);
}

void LogView::appendApiLog(const QString& type, const QString& payload) {
  if (payload.isEmpty()) {
    return;
  }
  if (payload.contains('\n') || payload.contains('\r')) {
    const QStringList lines =
        payload.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
    for (const auto& line : lines) {
      appendApiLog(type, line);
    }
    return;
  }
  const LogParser::LogKind kind = LogParser::parseLogKind(payload);
  LogParser::LogEntry      entry;
  entry.type = type.toLower();
  if (kind.isConnection && entry.type == "info") {
    QString label;
    if (!kind.protocol.isEmpty() && !kind.nodeName.isEmpty()) {
      label = QString("%1[%2]").arg(kind.protocol, kind.nodeName);
    } else if (!kind.protocol.isEmpty()) {
      label = kind.protocol;
    } else if (!kind.nodeName.isEmpty()) {
      label = QString("[%1]").arg(kind.nodeName);
    }
    if (kind.direction == "outbound") {
      if (!label.isEmpty() && !kind.host.isEmpty()) {
        entry.payload = QString("%1 -> %2").arg(label, kind.host);
      } else if (!kind.host.isEmpty()) {
        entry.payload = QString("outbound -> %1").arg(kind.host);
      } else {
        entry.payload = label.isEmpty() ? payload : label;
      }
    } else if (kind.direction == "inbound") {
      if (!label.isEmpty() && !kind.host.isEmpty()) {
        entry.payload = QString("%1 <- %2").arg(label, kind.host);
      } else if (!kind.host.isEmpty()) {
        entry.payload = QString("inbound <- %1").arg(kind.host);
      } else {
        entry.payload = label.isEmpty() ? payload : label;
      }
    } else {
      entry.payload = payload;
    }
    entry.direction = kind.direction;
    entry.network = kind.network;
  } else if (kind.isDns) {
    entry.payload   = payload;
    entry.direction = kind.direction;
  } else {
    entry.payload = payload;
    entry.direction.clear();
  }
  entry.timestamp = QDateTime::currentDateTime();
  m_store->append(entry);
}

void LogView::clear() {
  if (!m_store->clear()) {
    return;
  }
  m_forceRefresh = true;
  m_followTail = true;
  rebuildList();
}

void LogView::onFilterChanged() {
  m_forceRefresh = true;
  m_followTail = true;
  // Debounce typing and avoid rescanning the day for every keystroke.
  m_refreshTimer->start();
}

void LogView::onClearClicked() {
  clear();
}

void LogView::onCopyClicked() {
  QStringList lines;
  for (const auto& row : std::as_const(m_rows)) {
    const auto& log = row.entry;
    lines << QString("[%1] [%2] %3")
                 .arg(log.timestamp.toString("HH:mm:ss"),
                      log.type.toUpper(), log.payload);
  }
  QApplication::clipboard()->setText(lines.join("\n"));
}

void LogView::onExportClicked() {
  const QString path = QFileDialog::getSaveFileName(
      this, tr("Export Logs"), "logs.txt", tr("Text Files (*.txt)"));
  if (path.isEmpty()) {
    return;
  }
  m_exportBtn->setEnabled(false);
  m_exportBtn->setText(tr("Exporting..."));
  auto* watcher = new QFutureWatcher<QString>(this);
  connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
    m_exportBtn->setEnabled(true);
    m_exportBtn->setText(tr("Export"));
    const QString error = watcher->result();
    watcher->deleteLater();
    if (!error.isEmpty()) {
      QMessageBox::warning(this, tr("Export Logs"), error);
    }
  });
  watcher->setFuture(m_store->exportAsync(
      path, m_searchEdit->text().trimmed(), m_typeFilter->currentData().toString()));
}

void LogView::rebuildList() {
  m_store->flush();
  const QString search = m_searchEdit->text().trimmed();
  const QString type = m_typeFilter->currentData().toString();
  m_counts = m_store->counts(search, type);
  updateStats();
  auto* bar = m_scrollArea->verticalScrollBar();
  // Keep both row contents and their geometry unchanged while reading history.
  // New arrivals still go to disk; returning to the bottom loads the latest entries.
  if (!m_forceRefresh && !m_rows.isEmpty() &&
      (!m_followTail || bar->isSliderDown())) {
    return;
  }
  m_forceRefresh = false;
  auto next = m_store->latest(search, type);
  // Reuse overlapping rows on live updates instead of rebuilding every widget.
  m_listContainer->setUpdatesEnabled(false);
  qsizetype overlap = 0;
  if (!next.isEmpty()) {
    while (!m_rows.isEmpty() &&
           m_rows.first().id < next.first().id) {
      m_rows.removeFirst();
      removeFirstLogRow();
    }
    while (overlap < m_rows.size() && overlap < next.size() &&
           m_rows[overlap].id == next[overlap].id) ++overlap;
  }
  if (overlap != m_rows.size()) {
    clearListWidgets();
    overlap = 0;
  }
  for (qsizetype i = overlap; i < next.size(); ++i) {
    appendLogRow(next[i].entry);
  }
  m_rows = std::move(next);
  m_listContainer->setUpdatesEnabled(true);
  updateStats();
  updateEmptyState();
  if (m_store->error().isEmpty()) {
    m_storageError->hide();
  }
  // Word wrapping and window resizing can change the range in later layout
  // passes. rangeChanged will pin the bottom again after each such pass.
  scheduleTailScroll();
}

void LogView::updateStats() {
  m_totalTag->setText(tr("%1 entries").arg(m_counts.total));
  m_subtitleLabel->setText(tr("Kernel logs from the last 24 hours"));
  m_errorTag->setText(tr("Errors: %1").arg(m_counts.errors));
  m_warningTag->setText(tr("Warnings: %1").arg(m_counts.warnings));

}

void LogView::appendLogRow(const LogParser::LogEntry& entry) {
  m_listLayout->insertWidget(m_listLayout->count() - 1,
                             new LogRowWidget(entry));
}

void LogView::removeFirstLogRow() {
  if (m_listLayout->count() <= 1) {
    return;
  }
  QLayoutItem* item = m_listLayout->takeAt(0);
  if (item) {
    if (item->widget()) {
      delete item->widget();
    }
    delete item;
  }
}

void LogView::clearListWidgets() {
  while (m_listLayout->count() > 1) {
    QLayoutItem* item = m_listLayout->takeAt(0);
    if (item->widget()) {
      delete item->widget();
    }
    delete item;
  }
}

void LogView::updateEmptyState() {
  const QString query = m_searchEdit->text().trimmed();
  const bool    hasFilters =
      !query.isEmpty() || !m_typeFilter->currentData().toString().isEmpty();
  if (m_rows.isEmpty()) {
    m_emptyState->show();
    m_scrollArea->hide();
    m_emptyTitle->setText(hasFilters ? tr("No matching logs")
                                     : tr("No logs yet"));
  } else {
    m_emptyState->hide();
    m_scrollArea->show();
  }
}

void LogView::updateStyle() {
  ThemeService* ts = m_themeService;
  if (ts) {
    setStyleSheet(ts->loadStyleSheet(":/styles/log_view.qss"));
  }
}
