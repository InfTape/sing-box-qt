#include "LogView.h"
#include "utils/LogParser.h"
#include "app/interfaces/ThemeService.h"
#include "widgets/logs/LogRowWidget.h"
#include "widgets/common/MenuComboBox.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QStringList>
#include <QTextStream>
#include <QVBoxLayout>
#include <utility>


namespace {
constexpr int kMaxLogEntries = 20;
} // namespace

LogView::LogView(ThemeService *themeService, QWidget *parent)
    : QWidget(parent)
    , m_themeService(themeService)
{
    setupUI();
    updateStyle();

    if (m_themeService) {
        connect(m_themeService, &ThemeService::themeChanged,
                this, &LogView::updateStyle);
    }
}

void LogView::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    // Header
    QHBoxLayout *headerLayout = new QHBoxLayout;
    QVBoxLayout *titleLayout = new QVBoxLayout;
    titleLayout->setSpacing(4);

    m_titleLabel = new QLabel(tr("Logs"));
    m_titleLabel->setObjectName("PageTitle");
    m_subtitleLabel = new QLabel(tr("View kernel logs and errors"));
    m_subtitleLabel->setObjectName("PageSubtitle");

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_subtitleLabel);

    QWidget *controls = new QWidget;
    QHBoxLayout *controlsLayout = new QHBoxLayout(controls);
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

    m_autoScroll = new QCheckBox(tr("Auto scroll"));
    m_autoScroll->setObjectName("AutoScroll");
    m_autoScroll->setChecked(false);

    m_clearBtn = new QPushButton(tr("Clear"));
    m_clearBtn->setObjectName("ClearBtn");
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setFixedHeight(32);

    m_copyBtn = new QPushButton(tr("Copy"));
    m_copyBtn->setObjectName("CopyBtn");
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    m_copyBtn->setFixedHeight(32);

    m_exportBtn = new QPushButton(tr("Export"));
    m_exportBtn->setObjectName("ExportBtn");
    m_exportBtn->setCursor(Qt::PointingHandCursor);
    m_exportBtn->setFixedHeight(32);

    controlsLayout->addWidget(m_autoScroll);
    controlsLayout->addSpacing(10);
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
    QFrame *filterCard = new QFrame;
    filterCard->setObjectName("FilterCard");
    QHBoxLayout *filterLayout = new QHBoxLayout(filterCard);
    filterLayout->setContentsMargins(14, 12, 14, 12);
    filterLayout->setSpacing(12);

    m_searchEdit = new QLineEdit;
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
    QFrame *logCard = new QFrame;
    logCard->setObjectName("LogCard");
    QVBoxLayout *logCardLayout = new QVBoxLayout(logCard);
    logCardLayout->setContentsMargins(0, 0, 0, 0);
    logCardLayout->setSpacing(0);

    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_listContainer = new QWidget;
    m_listLayout = new QVBoxLayout(m_listContainer);
    m_listLayout->setContentsMargins(12, 12, 12, 12);
    m_listLayout->setSpacing(6);
    m_listLayout->addStretch();

    m_scrollArea->setWidget(m_listContainer);

    m_emptyState = new QFrame;
    m_emptyState->setObjectName("EmptyState");
    m_emptyState->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout *emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    emptyLayout->setSpacing(10);
    emptyLayout->setAlignment(Qt::AlignCenter);
    QLabel *emptyIcon = new QLabel(tr("!"));
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

    connect(m_searchEdit, &QLineEdit::textChanged, this, &LogView::onFilterChanged);
    connect(m_typeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LogView::onFilterChanged);
    connect(m_clearBtn, &QPushButton::clicked, this, &LogView::onClearClicked);
    connect(m_copyBtn, &QPushButton::clicked, this, &LogView::onCopyClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &LogView::onExportClicked);
}

void LogView::appendLog(const QString &message)
{
    const QString cleaned = LogParser::stripAnsiSequences(message).trimmed();
    if (cleaned.isEmpty()) return;
    if (cleaned.contains('\n') || cleaned.contains('\r')) {
        const QStringList lines = cleaned.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
        for (const auto &line : lines) {
            appendLog(line);
        }
        return;
    }
    const LogParser::LogKind kind = LogParser::parseLogKind(cleaned);

    LogParser::LogEntry entry;
    entry.type = LogParser::detectLogType(cleaned);
    if (kind.isConnection && entry.type == "info") {
        if (kind.direction == "outbound") {
            QString label;
            if (!kind.protocol.isEmpty() && !kind.nodeName.isEmpty()) {
                label = QString("%1[%2]").arg(kind.protocol, kind.nodeName);
            } else if (!kind.protocol.isEmpty()) {
                label = kind.protocol;
            } else if (!kind.nodeName.isEmpty()) {
                label = QString("[%1]").arg(kind.nodeName);
            }

            if (!kind.host.isEmpty()) {
                entry.payload = label.isEmpty() ? kind.host : QString("%1 -> %2").arg(label, kind.host);
            } else {
                entry.payload = label.isEmpty() ? cleaned : label;
            }
        } else {
            entry.payload = kind.host.isEmpty() ? cleaned : kind.host;
        }
        entry.direction = kind.direction;
    } else if (kind.isDns) {
        entry.payload = cleaned;
        entry.direction = kind.direction;
    } else {
        entry.payload = cleaned;
        entry.direction.clear();
    }
    entry.timestamp = QDateTime::currentDateTime();

    m_logs.push_back(entry);
    if (m_logs.size() > kMaxLogEntries) {
        const LogParser::LogEntry removed = m_logs.first();
        const bool removedMatches = logMatchesFilter(removed);
        m_logs.removeFirst();
        if (removedMatches && !m_filtered.isEmpty()) {
            m_filtered.removeFirst();
            removeFirstLogRow();
        }
    }

    if (logMatchesFilter(entry)) {
        m_filtered.push_back(entry);
        appendLogRow(entry);
    }

    updateStats();
    updateEmptyState();

    if (m_autoScroll->isChecked()) {
        m_scrollArea->verticalScrollBar()->setValue(m_scrollArea->verticalScrollBar()->maximum());
    }
}

void LogView::clear()
{
    m_logs.clear();
    m_filtered.clear();
    clearListWidgets();
    updateStats();
    updateEmptyState();
}

void LogView::onFilterChanged()
{
    rebuildList();
}

void LogView::onClearClicked()
{
    clear();
}

void LogView::onCopyClicked()
{
    QStringList lines;
    for (const auto &log : std::as_const(m_filtered)) {
        lines << QString("[%1] [%2] %3")
                    .arg(log.timestamp.toString("HH:mm:ss"))
                    .arg(log.type.toUpper())
                    .arg(log.payload);
    }
    QApplication::clipboard()->setText(lines.join("\n"));
}

void LogView::onExportClicked()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Logs"), "logs.txt", tr("Text Files (*.txt)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    for (const auto &log : std::as_const(m_filtered)) {
        out << QString("[%1] [%2] %3\n")
                   .arg(log.timestamp.toString(Qt::ISODate))
                   .arg(log.type.toUpper())
                   .arg(log.payload);
    }
}

void LogView::rebuildList()
{
    const QString query = m_searchEdit->text().trimmed();
    const QString typeValue = m_typeFilter->currentData().toString();

    m_filtered.clear();
    for (const auto &log : std::as_const(m_logs)) {
        const bool matchSearch = query.isEmpty()
            || log.payload.contains(query, Qt::CaseInsensitive);
        const bool matchType = typeValue.isEmpty() || log.type == typeValue;
        if (matchSearch && matchType) {
            m_filtered.push_back(log);
        }
    }

    clearListWidgets();

    for (const auto &log : std::as_const(m_filtered)) {
        appendLogRow(log);
    }

    updateStats();
    updateEmptyState();
}

void LogView::updateStats()
{
    int errorCount = 0;
    int warningCount = 0;
    for (const auto &log : std::as_const(m_filtered)) {
        if (log.type == "error" || log.type == "fatal" || log.type == "panic") errorCount++;
        if (log.type == "warning") warningCount++;
    }

    m_totalTag->setText(tr("%1 entries").arg(m_filtered.size()));
    m_errorTag->setText(tr("Errors: %1").arg(errorCount));
    m_warningTag->setText(tr("Warnings: %1").arg(warningCount));
    m_errorTag->setVisible(true);
    m_warningTag->setVisible(true);
}

bool LogView::logMatchesFilter(const LogParser::LogEntry &entry) const
{
    const QString query = m_searchEdit->text().trimmed();
    const QString typeValue = m_typeFilter->currentData().toString();
    const bool matchSearch = query.isEmpty()
        || entry.payload.contains(query, Qt::CaseInsensitive);
    const bool matchType = typeValue.isEmpty() || entry.type == typeValue;
    return matchSearch && matchType;
}

void LogView::appendLogRow(const LogParser::LogEntry &entry)
{
    m_listLayout->insertWidget(m_listLayout->count() - 1, new LogRowWidget(entry));
}

void LogView::removeFirstLogRow()
{
    if (m_listLayout->count() <= 1) {
        return;
    }
    QLayoutItem *item = m_listLayout->takeAt(0);
    if (item) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

void LogView::clearListWidgets()
{
    while (m_listLayout->count() > 1) {
        QLayoutItem *item = m_listLayout->takeAt(0);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

void LogView::updateEmptyState()
{
    const QString query = m_searchEdit->text().trimmed();
    const bool hasFilters = !query.isEmpty() || !m_typeFilter->currentData().toString().isEmpty();

    if (m_filtered.isEmpty()) {
        m_emptyState->show();
        m_scrollArea->hide();
        m_emptyTitle->setText(hasFilters ? tr("No matching logs") : tr("No logs yet"));
    } else {
        m_emptyState->hide();
        m_scrollArea->show();
    }
}


void LogView::updateStyle()
{
    ThemeService *ts = m_themeService;
    if (ts) setStyleSheet(ts->loadStyleSheet(":/styles/log_view.qss"));
}
