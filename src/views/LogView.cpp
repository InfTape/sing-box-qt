#include "LogView.h"
#include "utils/ThemeManager.h"
#include "widgets/RoundedMenu.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QFontMetrics>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QStringList>
#include <QTextStream>
#include <QVBoxLayout>
#include <QMenu>
#include <utility>


namespace {
constexpr int kMaxLogEntries = 20;

QString stripAnsiSequences(const QString &text)
{
    static const QRegularExpression kAnsiPattern("\x1B\\[[0-?]*[ -/]*[@-~]");
    QString cleaned = text;
    cleaned.remove(kAnsiPattern);
    return cleaned;
}

QString normalizeHostToken(const QString &token)
{
    if (token.isEmpty()) return token;
    if (token.startsWith('[')) {
        return token;
    }
    return token;
}

struct LogKind {
    QString direction;
    QString host;
    QString nodeName;
    QString protocol;
    bool isConnection = false;
    bool isDns = false;
};

LogKind parseLogKind(const QString &message)
{
    LogKind info;
    static const QRegularExpression kDnsPattern("\\bdns\\s*:");
    static const QRegularExpression kOutboundNode(R"(outbound/([^\[]+)\[([^\]]+)\])");
    if (message.contains(kDnsPattern)) {
        info.direction = "dns";
        info.isDns = true;
        return info;
    }

    if (message.contains("inbound connection")) {
        info.direction = "inbound";
    } else if (message.contains("outbound connection")) {
        info.direction = "outbound";
    } else {
        return info;
    }

    static const QRegularExpression kConnHost(R"(connection (?:from|to) ([^\s]+))");
    const QRegularExpressionMatch match = kConnHost.match(message);
    if (match.hasMatch()) {
        info.host = normalizeHostToken(match.captured(1));
    }

    if (info.direction == "outbound") {
        const QRegularExpressionMatch nodeMatch = kOutboundNode.match(message);
        if (nodeMatch.hasMatch()) {
            info.protocol = nodeMatch.captured(1).trimmed();
            info.nodeName = nodeMatch.captured(2).trimmed();
        }
    }

    info.isConnection = true;
    return info;
}

class MenuComboBox : public QComboBox
{
public:
    explicit MenuComboBox(QWidget *parent = nullptr)
        : QComboBox(parent)
    {
        m_menu = new RoundedMenu(this);
        m_menu->setObjectName("ComboMenu");
        updateMenuStyle();

        ThemeManager &tm = ThemeManager::instance();
        connect(&tm, &ThemeManager::themeChanged, this, [this]() {
            updateMenuStyle();
        });
    }

protected:
    void showPopup() override
    {
        if (!m_menu) return;
        m_menu->clear();

        for (int i = 0; i < count(); ++i) {
            QAction *action = m_menu->addAction(itemText(i));
            action->setCheckable(true);
            action->setChecked(i == currentIndex());
            connect(action, &QAction::triggered, this, [this, i]() {
                setCurrentIndex(i);
            });
        }

        const int menuWidth = qMax(width(), 180);
        m_menu->setFixedWidth(menuWidth);
        m_menu->popup(mapToGlobal(QPoint(0, height())));
    }

    void hidePopup() override
    {
        if (m_menu) {
            m_menu->hide();
        }
    }

private:
    void updateMenuStyle()
    {
        if (!m_menu) return;
        ThemeManager &tm = ThemeManager::instance();
        m_menu->setThemeColors(tm.getColor("bg-secondary"), tm.getColor("primary"));
        m_menu->setStyleSheet(QString(R"(
            #ComboMenu {
                background: transparent;
                border: none;
                padding: 6px;
            }
            #ComboMenu::panel {
                background: transparent;
                border: none;
            }
            #ComboMenu::item {
                color: %1;
                padding: 8px 14px;
                border-radius: 10px;
            }
            #ComboMenu::indicator {
                width: 14px;
                height: 14px;
            }
            #ComboMenu::indicator:checked {
                image: url(:/icons/check.svg);
            }
            #ComboMenu::indicator:unchecked {
                image: none;
            }
            #ComboMenu::item:selected {
                background-color: %2;
                color: white;
            }
            #ComboMenu::item:checked {
                color: %4;
            }
            #ComboMenu::item:checked:selected {
                color: %4;
            }
            #ComboMenu::separator {
                height: 1px;
                background-color: %3;
                margin: 6px 4px;
            }
        )")
        .arg(tm.getColorString("text-primary"))
        .arg(tm.getColorString("bg-tertiary"))
        .arg(tm.getColorString("border"))
        .arg(tm.getColorString("primary")));
    }

    RoundedMenu *m_menu = nullptr;
};
} // namespace

LogView::LogView(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    updateStyle();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &LogView::updateStyle);
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
    m_totalTag->setObjectName("TagInfo");
    m_errorTag = new QLabel(tr("Errors: 0"));
    m_errorTag->setObjectName("TagError");
    m_warningTag = new QLabel(tr("Warnings: 0"));
    m_warningTag->setObjectName("TagWarning");

    m_autoScroll = new QCheckBox(tr("Auto scroll"));
    m_autoScroll->setObjectName("AutoScroll");
    m_autoScroll->setChecked(false);

    m_clearBtn = new QPushButton(tr("Clear"));
    m_clearBtn->setObjectName("ActionDangerBtn");
    m_clearBtn->setCursor(Qt::PointingHandCursor);

    m_copyBtn = new QPushButton(tr("Copy"));
    m_copyBtn->setObjectName("ActionBtn");
    m_copyBtn->setCursor(Qt::PointingHandCursor);

    m_exportBtn = new QPushButton(tr("Export"));
    m_exportBtn->setObjectName("ActionBtn");
    m_exportBtn->setCursor(Qt::PointingHandCursor);

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

    m_typeFilter = new MenuComboBox;
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
    const QString cleaned = stripAnsiSequences(message).trimmed();
    if (cleaned.isEmpty()) return;
    if (cleaned.contains('\n') || cleaned.contains('\r')) {
        const QStringList lines = cleaned.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
        for (const auto &line : lines) {
            appendLog(line);
        }
        return;
    }
    const LogKind kind = parseLogKind(cleaned);

    LogEntry entry;
    entry.type = detectLogType(cleaned);
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
        const LogEntry removed = m_logs.first();
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
    m_errorTag->setVisible(errorCount > 0);
    m_warningTag->setVisible(warningCount > 0);
}

bool LogView::logMatchesFilter(const LogEntry &entry) const
{
    const QString query = m_searchEdit->text().trimmed();
    const QString typeValue = m_typeFilter->currentData().toString();
    const bool matchSearch = query.isEmpty()
        || entry.payload.contains(query, Qt::CaseInsensitive);
    const bool matchType = typeValue.isEmpty() || entry.type == typeValue;
    return matchSearch && matchType;
}

void LogView::appendLogRow(const LogEntry &entry)
{
    m_listLayout->insertWidget(m_listLayout->count() - 1, createLogRow(entry));
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

QWidget* LogView::createLogRow(const LogEntry &entry)
{
    QFrame *row = new QFrame;
    row->setObjectName("LogEntry");
    row->setProperty("logType", entry.type);

    QHBoxLayout *layout = new QHBoxLayout(row);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(10);

    QLabel *timeLabel = new QLabel(entry.timestamp.toString("HH:mm:ss"));
    timeLabel->setObjectName("LogTime");

    const int badgePaddingX = 6;
    const int badgePaddingY = 2;

    QLabel *typeLabel = new QLabel(logTypeLabel(entry.type));
    typeLabel->setObjectName("LogBadge");
    typeLabel->setProperty("logType", entry.type);
    typeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    typeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    const QFontMetrics typeMetrics(typeLabel->font());
    const QSize typeSize = typeMetrics.size(Qt::TextSingleLine, typeLabel->text());
    typeLabel->setFixedSize(typeSize.width() + badgePaddingX * 2,
                            typeSize.height() + badgePaddingY * 2);

    QHBoxLayout *badgeLayout = new QHBoxLayout;
    badgeLayout->setContentsMargins(0, 0, 0, 0);
    badgeLayout->setSpacing(6);
    badgeLayout->addWidget(typeLabel);

    if (!entry.direction.isEmpty()) {
        QString directionLabel;
        if (entry.direction == "outbound") {
            directionLabel = tr("Outbound");
        } else if (entry.direction == "inbound") {
            directionLabel = tr("Inbound");
        } else if (entry.direction == "dns") {
            directionLabel = tr("DNS");
        } else {
            directionLabel = entry.direction.toUpper();
        }
        QLabel *directionTag = new QLabel(directionLabel);
        directionTag->setObjectName("LogBadge");
        directionTag->setProperty("logType", "info");
        directionTag->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        directionTag->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        const QFontMetrics dirMetrics(directionTag->font());
        const QSize dirSize = dirMetrics.size(Qt::TextSingleLine, directionTag->text());
        directionTag->setFixedSize(dirSize.width() + badgePaddingX * 2,
                                   dirSize.height() + badgePaddingY * 2);
        badgeLayout->addWidget(directionTag);
    }

    QWidget *badgeRow = new QWidget;
    badgeRow->setLayout(badgeLayout);

    QLabel *content = new QLabel(entry.payload);
    content->setObjectName("LogContent");
    content->setWordWrap(true);
    content->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    layout->addWidget(timeLabel, 0, Qt::AlignTop);
    layout->addWidget(badgeRow, 0, Qt::AlignTop);
    layout->addWidget(content, 1);

    return row;
}

QString LogView::detectLogType(const QString &message) const
{
    const QString upper = message.toUpper();
    static const QRegularExpression kPanicRe("\\bPANIC\\b");
    static const QRegularExpression kFatalRe("\\bFATAL\\b");
    static const QRegularExpression kErrorRe("\\bERROR\\b");
    static const QRegularExpression kWarnRe("\\bWARN\\b");
    static const QRegularExpression kWarningRe("\\bWARNING\\b");
    static const QRegularExpression kDebugRe("\\bDEBUG\\b");
    static const QRegularExpression kTraceRe("\\bTRACE\\b");
    static const QRegularExpression kInfoRe("\\bINFO\\b");

    if (upper.contains(kPanicRe)) return "panic";
    if (upper.contains(kFatalRe)) return "fatal";
    if (upper.contains(kErrorRe)) return "error";
    if (upper.contains(kWarnRe) || upper.contains(kWarningRe)) return "warning";
    if (upper.contains(kDebugRe)) return "debug";
    if (upper.contains(kTraceRe)) return "trace";
    if (upper.contains(kInfoRe)) return "info";
    return "info";
}

QString LogView::logTypeLabel(const QString &type) const
{
    if (type == "trace") return "TRACE";
    if (type == "debug") return "DEBUG";
    if (type == "info") return "INFO";
    if (type == "warning") return "WARN";
    if (type == "error") return "ERROR";
    if (type == "fatal") return "FATAL";
    if (type == "panic") return "PANIC";
    return "INFO";
}

void LogView::updateStyle()
{
    ThemeManager &tm = ThemeManager::instance();

    setStyleSheet(QString(R"(
        #PageTitle {
            font-size: 22px;
            font-weight: 700;
            color: %1;
        }
        #PageSubtitle {
            font-size: 13px;
            color: %2;
        }
        #FilterCard, #LogCard {
            background-color: %3;
            border: 1px solid %4;
            border-radius: 16px;
        }
        #LogCard QScrollArea {
            background: transparent;
            border: none;
        }
        #LogCard QScrollArea > QWidget > QWidget {
            background: transparent;
        }
        #SearchInput, #FilterSelect {
            background-color: %5;
            border: 1px solid #353b43;
            border-radius: 12px;
            padding: 8px 12px;
            color: %1;
        }
        #SearchInput:focus, #FilterSelect:focus {
            border-color: #353b43;
        }
        #TagInfo, #TagError, #TagWarning {
            padding: 6px 20px;
            border-radius: 10px;
            font-size: 12px;
            font-weight: 600;
        }
        #TagInfo { color: %1; background: %5; }
        #TagError { color: #ef4444; background: rgba(239,68,68,0.12); }
        #TagWarning { color: #f59e0b; background: rgba(245,158,11,0.12); }
        #AutoScroll {
            color: %1;
        }
        #ActionBtn {
            background-color: %5;
            color: %1;
            border: 1px solid #353b43;
            border-radius: 10px;
            padding: 6px 14px;
        }
        #ActionBtn:hover {
            background-color: %6;
            color: white;
        }
        #ActionDangerBtn {
            background-color: %7;
            color: white;
            border: none;
            border-radius: 10px;
            padding: 6px 14px;
        }
        #ActionDangerBtn:hover {
            background-color: %8;
        }
        #LogEntry {
            background: transparent;
            border-radius: 8px;
        }
        #LogEntry:hover {
            background-color: %5;
        }
        #LogTime {
            font-size: 11px;
            color: %2;
        }
        #LogBadge {
            font-size: 10px;
            font-weight: 600;
            padding: 2px 6px;
            border-radius: 4px;
        }
        #LogBadge[logType="info"] {
            color: #3b82f6;
            background: rgba(59,130,246,0.12);
        }
        #LogBadge[logType="warning"] {
            color: #f59e0b;
            background: rgba(245,158,11,0.12);
        }
        #LogBadge[logType="error"] {
            color: #ef4444;
            background: rgba(239,68,68,0.12);
        }
        #LogBadge[logType="debug"] {
            color: %2;
            background: %5;
        }
        #LogBadge[logType="trace"] {
            color: %2;
            background: %5;
        }
        #LogBadge[logType="fatal"] {
            color: #ef4444;
            background: rgba(239,68,68,0.12);
        }
        #LogBadge[logType="panic"] {
            color: #ef4444;
            background: rgba(239,68,68,0.12);
        }
        #LogContent {
            color: %2;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 12px;
        }
        #LogEntry[logType="error"] #LogContent,
        #LogEntry[logType="fatal"] #LogContent,
        #LogEntry[logType="panic"] #LogContent {
            color: #ef4444;
        }
        #EmptyTitle {
            font-size: 16px;
            color: %1;
        }
        #EmptyIcon {
            font-size: 24px;
        }
        #EmptyState {
            background: transparent;
        }
    )")
    .arg(tm.getColorString("text-primary"))
    .arg(tm.getColorString("text-secondary"))
    .arg(tm.getColorString("bg-secondary"))
    .arg(tm.getColorString("border"))
    .arg(tm.getColorString("panel-bg"))
    .arg(tm.getColorString("primary"))
    .arg(tm.getColorString("error"))
    .arg(tm.getColorString("error") + "cc"));
}
