#include "RulesView.h"
#include "core/ProxyService.h"
#include "utils/ThemeManager.h"
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QStringList>
#include <QVBoxLayout>
#include <utility>

RulesView::RulesView(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    updateStyle();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &RulesView::updateStyle);
}

void RulesView::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    // Header
    QHBoxLayout *headerLayout = new QHBoxLayout;
    QVBoxLayout *titleLayout = new QVBoxLayout;
    titleLayout->setSpacing(4);

    m_titleLabel = new QLabel(tr("规则"));
    m_titleLabel->setObjectName("PageTitle");
    m_subtitleLabel = new QLabel(tr("查看并筛选当前规则列表"));
    m_subtitleLabel->setObjectName("PageSubtitle");

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_subtitleLabel);

    m_refreshBtn = new QPushButton(tr("获取规则"));
    m_refreshBtn->setObjectName("PrimaryActionBtn");
    m_refreshBtn->setCursor(Qt::PointingHandCursor);
    m_refreshBtn->setMinimumSize(110, 36);

    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    headerLayout->addWidget(m_refreshBtn);

    mainLayout->addLayout(headerLayout);

    // Filters
    QFrame *filterCard = new QFrame;
    filterCard->setObjectName("FilterCard");
    QHBoxLayout *filterLayout = new QHBoxLayout(filterCard);
    filterLayout->setContentsMargins(14, 12, 14, 12);
    filterLayout->setSpacing(12);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setObjectName("SearchInput");
    m_searchEdit->setPlaceholderText(tr("搜索规则内容或代理..."));
    m_searchEdit->setClearButtonEnabled(true);

    m_typeFilter = new QComboBox;
    m_typeFilter->setObjectName("FilterSelect");

    m_proxyFilter = new QComboBox;
    m_proxyFilter->setObjectName("FilterSelect");

    filterLayout->addWidget(m_searchEdit, 2);
    filterLayout->addWidget(m_typeFilter, 1);
    filterLayout->addWidget(m_proxyFilter, 1);

    mainLayout->addWidget(filterCard);

    // Rules list
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setObjectName("RulesScroll");

    m_gridContainer = new QWidget;
    m_gridLayout = new QGridLayout(m_gridContainer);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    m_gridLayout->setSpacing(16);

    m_scrollArea->setWidget(m_gridContainer);

    m_emptyState = new QFrame;
    m_emptyState->setObjectName("EmptyState");
    QVBoxLayout *emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    emptyLayout->setSpacing(10);
    emptyLayout->setAlignment(Qt::AlignCenter);

    QLabel *emptyIcon = new QLabel(tr("🔍"));
    emptyIcon->setObjectName("EmptyIcon");
    emptyIcon->setAlignment(Qt::AlignCenter);

    m_emptyTitle = new QLabel(tr("暂无规则数据"));
    m_emptyTitle->setObjectName("EmptyTitle");
    m_emptyTitle->setAlignment(Qt::AlignCenter);

    m_emptyAction = new QPushButton(tr("获取规则"));
    m_emptyAction->setObjectName("PrimaryActionBtn");
    m_emptyAction->setCursor(Qt::PointingHandCursor);
    m_emptyAction->setMinimumSize(110, 36);

    emptyLayout->addWidget(emptyIcon);
    emptyLayout->addWidget(m_emptyTitle);
    emptyLayout->addWidget(m_emptyAction, 0, Qt::AlignCenter);

    mainLayout->addWidget(m_scrollArea, 1);
    mainLayout->addWidget(m_emptyState, 1);

    connect(m_refreshBtn, &QPushButton::clicked, this, &RulesView::onRefreshClicked);
    connect(m_emptyAction, &QPushButton::clicked, this, &RulesView::onEmptyActionClicked);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &RulesView::onSearchChanged);
    connect(m_typeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RulesView::onFilterChanged);
    connect(m_proxyFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RulesView::onFilterChanged);
}

void RulesView::setProxyService(ProxyService *service)
{
    m_proxyService = service;
    if (!m_proxyService) return;

    connect(m_proxyService, &ProxyService::rulesReceived, this, [this](const QJsonArray &rules) {
        m_rules.clear();
        m_rules.reserve(rules.count());
        for (const auto &item : rules) {
            const QJsonObject rule = item.toObject();
            RuleItem data;
            data.type = rule.value("type").toString();
            data.payload = rule.value("payload").toString();
            data.proxy = rule.value("proxy").toString();
            m_rules.push_back(data);
        }

        m_loading = false;
        m_refreshBtn->setEnabled(true);
        m_refreshBtn->setText(tr("获取规则"));
        updateFilterOptions();
        applyFilters();
    });
}

void RulesView::refresh()
{
    if (!m_proxyService) return;
    m_loading = true;
    m_refreshBtn->setEnabled(false);
    m_refreshBtn->setText(tr("加载中..."));
    m_proxyService->fetchRules();
}

void RulesView::onRefreshClicked()
{
    refresh();
}

void RulesView::onEmptyActionClicked()
{
    const bool hasFilters = !m_searchEdit->text().trimmed().isEmpty()
        || !m_typeFilter->currentData().toString().isEmpty()
        || !m_proxyFilter->currentData().toString().isEmpty();
    if (hasFilters) {
        m_searchEdit->clear();
        m_typeFilter->setCurrentIndex(0);
        m_proxyFilter->setCurrentIndex(0);
    } else {
        refresh();
    }
}

void RulesView::onSearchChanged()
{
    applyFilters();
}

void RulesView::onFilterChanged()
{
    applyFilters();
}

void RulesView::applyFilters()
{
    const QString query = m_searchEdit->text().trimmed();
    const QString typeValue = m_typeFilter->currentData().toString();
    const QString proxyValue = m_proxyFilter->currentData().toString();

    m_filteredRules.clear();
    for (const auto &rule : std::as_const(m_rules)) {
        const bool matchSearch = query.isEmpty()
            || rule.payload.contains(query, Qt::CaseInsensitive)
            || rule.proxy.contains(query, Qt::CaseInsensitive);
        const bool matchType = typeValue.isEmpty() || rule.type == typeValue;
        const bool matchProxy = proxyValue.isEmpty()
            || normalizeProxyValue(rule.proxy) == proxyValue;

        if (matchSearch && matchType && matchProxy) {
            m_filteredRules.push_back(rule);
        }
    }

    rebuildGrid();
    updateEmptyState();
}

void RulesView::updateFilterOptions()
{
    const QString currentType = m_typeFilter->currentData().toString();
    const QString currentProxy = m_proxyFilter->currentData().toString();

    QSet<QString> types;
    QSet<QString> proxies;
    for (const auto &rule : std::as_const(m_rules)) {
        types.insert(rule.type);
        proxies.insert(normalizeProxyValue(rule.proxy));
    }

    m_typeFilter->blockSignals(true);
    m_typeFilter->clear();
    m_typeFilter->addItem(tr("类型"), QString());
    QStringList typeList = types.values();
    typeList.sort();
    for (const auto &type : typeList) {
        m_typeFilter->addItem(type, type);
    }
    int typeIndex = m_typeFilter->findData(currentType);
    m_typeFilter->setCurrentIndex(typeIndex < 0 ? 0 : typeIndex);
    m_typeFilter->blockSignals(false);

    m_proxyFilter->blockSignals(true);
    m_proxyFilter->clear();
    m_proxyFilter->addItem(tr("目标代理"), QString());
    if (proxies.contains("direct")) {
        m_proxyFilter->addItem(tr("直连"), "direct");
    }
    if (proxies.contains("reject")) {
        m_proxyFilter->addItem(tr("拦截"), "reject");
    }
    QStringList proxyList = proxies.values();
    proxyList.sort();
    for (const auto &proxy : proxyList) {
        if (proxy == "direct" || proxy == "reject") continue;
        m_proxyFilter->addItem(proxy, proxy);
    }
    int proxyIndex = m_proxyFilter->findData(currentProxy);
    m_proxyFilter->setCurrentIndex(proxyIndex < 0 ? 0 : proxyIndex);
    m_proxyFilter->blockSignals(false);
}

void RulesView::rebuildGrid()
{
    while (m_gridLayout->count() > 0) {
        QLayoutItem *item = m_gridLayout->takeAt(0);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    const int availableWidth = m_scrollArea->viewport()->width();
    const int cardWidth = 320;
    const int columns = qMax(1, availableWidth / cardWidth);
    m_columnCount = columns;

    int row = 0;
    int col = 0;
    for (int i = 0; i < m_filteredRules.size(); ++i) {
        QWidget *card = createRuleCard(m_filteredRules[i], i + 1);
        m_gridLayout->addWidget(card, row, col);
        col++;
        if (col >= columns) {
            col = 0;
            row++;
        }
    }
    m_gridLayout->setColumnStretch(columns, 1);
}

void RulesView::updateEmptyState()
{
    const bool hasFilters = !m_searchEdit->text().trimmed().isEmpty()
        || !m_typeFilter->currentData().toString().isEmpty()
        || !m_proxyFilter->currentData().toString().isEmpty();

    if (m_filteredRules.isEmpty()) {
        m_emptyState->show();
        m_scrollArea->hide();
        m_emptyTitle->setText(hasFilters ? tr("没有匹配的规则") : tr("暂无规则数据"));
        m_emptyAction->setText(hasFilters ? tr("清空筛选") : tr("获取规则"));
    } else {
        m_emptyState->hide();
        m_scrollArea->show();
    }
}

QWidget* RulesView::createRuleCard(const RuleItem &rule, int index)
{
    QFrame *card = new QFrame;
    card->setObjectName("RuleCard");

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    QHBoxLayout *headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(6);

    QLabel *typeTag = new QLabel(rule.type);
    typeTag->setObjectName("RuleTag");
    typeTag->setProperty("tagType", ruleTagType(rule.type));

    QLabel *indexLabel = new QLabel(QString("#%1").arg(index));
    indexLabel->setObjectName("RuleIndex");

    headerLayout->addWidget(typeTag);
    headerLayout->addStretch();
    headerLayout->addWidget(indexLabel);

    QVBoxLayout *bodyLayout = new QVBoxLayout;
    bodyLayout->setSpacing(6);

    QLabel *contentLabel = new QLabel(tr("内容"));
    contentLabel->setObjectName("RuleLabel");
    QLabel *contentValue = new QLabel(rule.payload);
    contentValue->setObjectName("RuleValue");
    contentValue->setWordWrap(true);

    QLabel *proxyLabel = new QLabel(tr("目标代理"));
    proxyLabel->setObjectName("RuleLabel");

    QLabel *proxyValue = new QLabel(displayProxyLabel(rule.proxy));
    proxyValue->setObjectName("RuleProxyTag");
    proxyValue->setProperty("tagType", proxyTagType(rule.proxy));

    bodyLayout->addWidget(contentLabel);
    bodyLayout->addWidget(contentValue);
    bodyLayout->addWidget(proxyLabel);
    bodyLayout->addWidget(proxyValue);

    layout->addLayout(headerLayout);
    layout->addLayout(bodyLayout);

    return card;
}

QString RulesView::normalizeProxyValue(const QString &proxy) const
{
    QString value = proxy.trimmed();
    if (value.compare("direct", Qt::CaseInsensitive) == 0) return "direct";
    if (value.compare("reject", Qt::CaseInsensitive) == 0) return "reject";
    if (value.startsWith('[') && value.endsWith(']')) {
        value = value.mid(1, value.length() - 2);
    }
    if (value.startsWith("Proxy(") && value.endsWith(")")) {
        value = value.mid(6, value.length() - 7);
    }
    return value;
}

QString RulesView::displayProxyLabel(const QString &proxy) const
{
    const QString value = normalizeProxyValue(proxy);
    if (value == "direct") return tr("直连");
    if (value == "reject") return tr("拦截");
    return value;
}

QString RulesView::ruleTagType(const QString &type) const
{
    const QString lower = type.toLower();
    if (lower.contains("domain")) return "info";
    if (lower.contains("ipcidr")) return "success";
    if (lower.contains("source")) return "warning";
    if (lower.contains("port")) return "error";
    return "default";
}

QString RulesView::proxyTagType(const QString &proxy) const
{
    const QString value = normalizeProxyValue(proxy);
    if (value == "direct") return "success";
    if (value == "reject") return "error";
    return "info";
}

void RulesView::updateStyle()
{
    ThemeManager &tm = ThemeManager::instance();

    const QString style = QString(R"(
        #PageTitle {
            font-size: 22px;
            font-weight: 700;
            color: %1;
        }
        #PageSubtitle {
            font-size: 13px;
            color: %2;
        }
        #PrimaryActionBtn {
            background-color: %3;
            color: white;
            border-radius: 12px;
            padding: 8px 18px;
            border: none;
        }
        #PrimaryActionBtn:hover {
            background-color: %4;
        }
        #FilterCard {
            background-color: %5;
            border: 1px solid %6;
            border-radius: 16px;
        }
        #SearchInput, #FilterSelect {
            background-color: %7;
            border: 1px solid %6;
            border-radius: 12px;
            padding: 8px 12px;
            color: %1;
        }
        #SearchInput:focus, #FilterSelect:focus {
            border-color: %3;
        }
        #RuleCard {
            background-color: %7;
            border: 1px solid %6;
            border-radius: 16px;
        }
        #RuleCard:hover {
            border-color: %3;
        }
        #RuleTag {
            padding: 4px 8px;
            border-radius: 10px;
            font-size: 11px;
            font-weight: 600;
        }
        #RuleTag[tagType="info"] { color: #3b82f6; background: rgba(59,130,246,0.12); }
        #RuleTag[tagType="success"] { color: #10b981; background: rgba(16,185,129,0.12); }
        #RuleTag[tagType="warning"] { color: #f59e0b; background: rgba(245,158,11,0.12); }
        #RuleTag[tagType="error"] { color: #ef4444; background: rgba(239,68,68,0.12); }
        #RuleTag[tagType="default"] { color: %2; background: %5; }
        #RuleIndex {
            font-size: 12px;
            color: %2;
        }
        #RuleLabel {
            font-size: 11px;
            text-transform: uppercase;
            color: %2;
            letter-spacing: 0.05em;
        }
        #RuleValue {
            font-size: 13px;
            color: %1;
            font-family: 'Consolas', 'Monaco', monospace;
        }
        #RuleProxyTag {
            padding: 4px 8px;
            border-radius: 10px;
            font-size: 11px;
            font-weight: 600;
        }
        #RuleProxyTag[tagType="success"] { color: #10b981; background: rgba(16,185,129,0.12); }
        #RuleProxyTag[tagType="error"] { color: #ef4444; background: rgba(239,68,68,0.12); }
        #RuleProxyTag[tagType="info"] { color: #3b82f6; background: rgba(59,130,246,0.12); }
        #EmptyState {
            background: transparent;
        }
        #EmptyTitle {
            font-size: 16px;
            color: %1;
        }
        #EmptyIcon {
            font-size: 24px;
        }
    )")
    .arg(tm.getColorString("text-primary"))
    .arg(tm.getColorString("text-secondary"))
    .arg(tm.getColorString("primary"))
    .arg(tm.getColorString("primary") + "cc")
    .arg(tm.getColorString("bg-tertiary"))
    .arg(tm.getColorString("border"))
    .arg(tm.getColorString("panel-bg"));

    setStyleSheet(style);
}

void RulesView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_filteredRules.isEmpty()) {
        rebuildGrid();
    }
}
