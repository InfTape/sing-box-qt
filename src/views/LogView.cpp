#include "LogView.h"
#include "utils/ThemeManager.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStringList>
#include <QTextStream>
#include <QVBoxLayout>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <utility>

namespace {
constexpr int kMaxLogEntries = 20;

class RoundedMenu : public QMenu
{
public:
    explicit RoundedMenu(QWidget *parent = nullptr)
        : QMenu(parent)
    {
        setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
    }

    void setThemeColors(const QColor &bg, const QColor &border)
    {
        m_bgColor = bg;
        m_borderColor = border;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QRectF rect = this->rect().adjusted(1, 1, -1, -1);
        QPainterPath path;
        path.addRoundedRect(rect, 10, 10);

        painter.fillPath(path, m_bgColor);
        painter.setPen(QPen(m_borderColor, 1));
        painter.drawPath(path);

        QMenu::paintEvent(event);
    }

private:
    QColor m_bgColor = QColor(30, 41, 59);
    QColor m_borderColor = QColor(90, 169, 255, 180);
};

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
}

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

    m_titleLabel = new QLabel(tr("日志"));
    m_titleLabel->setObjectName("PageTitle");
    m_subtitleLabel = new QLabel(tr("查看内核运行日志与错误信息"));
    m_subtitleLabel->setObjectName("PageSubtitle");

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_subtitleLabel);

    QWidget *controls = new QWidget;
    QHBoxLayout *controlsLayout = new QHBoxLayout(controls);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(8);

    m_totalTag = new QLabel(tr("0 条"));
    m_totalTag->setObjectName("TagInfo");
    m_errorTag = new QLabel(tr("错误: 0"));
    m_errorTag->setObjectName("TagError");
    m_warningTag = new QLabel(tr("警告: 0"));
    m_warningTag->setObjectName("TagWarning");

    m_autoScroll = new QCheckBox(tr("自动滚动"));
    m_autoScroll->setObjectName("AutoScroll");
    m_autoScroll->setChecked(false);

    m_clearBtn = new QPushButton(tr("清空"));
    m_clearBtn->setObjectName("ActionDangerBtn");
    m_clearBtn->setCursor(Qt::PointingHandCursor);

    m_copyBtn = new QPushButton(tr("复制"));
    m_copyBtn->setObjectName("ActionBtn");
    m_copyBtn->setCursor(Qt::PointingHandCursor);

    m_exportBtn = new QPushButton(tr("导出"));
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
    m_searchEdit->setPlaceholderText(tr("搜索日志内容..."));
    m_searchEdit->setClearButtonEnabled(true);

    m_typeFilter = new MenuComboBox;
    m_typeFilter->setObjectName("FilterSelect");
    m_typeFilter->addItem(tr("类型"), QString());
    m_typeFilter->addItem("DEBUG", "debug");
    m_typeFilter->addItem("INFO", "info");
    m_typeFilter->addItem("WARN", "warning");
    m_typeFilter->addItem("ERROR", "error");

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
    QLabel *emptyIcon = new QLabel(tr("📝"));
    emptyIcon->setObjectName("EmptyIcon");
    emptyIcon->setAlignment(Qt::AlignCenter);
    m_emptyTitle = new QLabel(tr("暂无日志"));
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
    LogEntry entry;
    entry.type = detectLogType(message);
    entry.payload = message;
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
    const QString path = QFileDialog::getSaveFileName(this, tr("导出日志"), "logs.txt", tr("文本文件 (*.txt)"));
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
        if (log.type == "error") errorCount++;
        if (log.type == "warning") warningCount++;
    }

    m_totalTag->setText(tr("%1 条").arg(m_filtered.size()));
    m_errorTag->setText(tr("错误: %1").arg(errorCount));
    m_warningTag->setText(tr("警告: %1").arg(warningCount));
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
        m_emptyTitle->setText(hasFilters ? tr("没有匹配的日志") : tr("暂无日志"));
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

    QWidget *meta = new QWidget;
    QVBoxLayout *metaLayout = new QVBoxLayout(meta);
    metaLayout->setContentsMargins(0, 0, 0, 0);
    metaLayout->setSpacing(2);

    QLabel *timeLabel = new QLabel(entry.timestamp.toString("HH:mm:ss"));
    timeLabel->setObjectName("LogTime");

    QLabel *typeLabel = new QLabel(logTypeLabel(entry.type));
    typeLabel->setObjectName("LogBadge");
    typeLabel->setProperty("logType", entry.type);

    metaLayout->addWidget(timeLabel);
    metaLayout->addWidget(typeLabel);

    QLabel *content = new QLabel(entry.payload);
    content->setObjectName("LogContent");
    content->setWordWrap(true);

    layout->addWidget(meta);
    layout->addWidget(content, 1);

    return row;
}

QString LogView::detectLogType(const QString &message) const
{
    const QString upper = message.toUpper();
    if (upper.contains("[ERROR]") || upper.contains(" ERROR ")) return "error";
    if (upper.contains("[WARN]") || upper.contains(" WARNING ")) return "warning";
    if (upper.contains("[DEBUG]") || upper.contains(" DEBUG ")) return "debug";
    if (upper.contains("[INFO]") || upper.contains(" INFO ")) return "info";
    return "info";
}

QString LogView::logTypeLabel(const QString &type) const
{
    if (type == "error") return tr("错误");
    if (type == "warning") return tr("警告");
    if (type == "debug") return "DEBUG";
    return tr("信息");
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
        #LogContent {
            color: %2;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 12px;
        }
        #LogEntry[logType="error"] #LogContent {
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
