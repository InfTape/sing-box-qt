#include "RulesView.h"
#include "core/ProxyService.h"
#include "dialogs/RuleEditorDialog.h"
#include "services/RuleConfigService.h"
#include "utils/RuleUtils.h"
#include "utils/ThemeManager.h"
#include "widgets/MenuComboBox.h"
#include "widgets/RuleCard.h"
#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QStringList>
#include <QVBoxLayout>
#include <algorithm>
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

    m_titleLabel = new QLabel(tr("Rules"));
    m_titleLabel->setObjectName("PageTitle");
    m_subtitleLabel = new QLabel(tr("View and filter the current rule list"));
    m_subtitleLabel->setObjectName("PageSubtitle");

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_subtitleLabel);

    m_refreshBtn = new QPushButton(tr("Fetch Rules"));
    m_refreshBtn->setObjectName("PrimaryActionBtn");
    m_refreshBtn->setCursor(Qt::PointingHandCursor);
    m_refreshBtn->setMinimumSize(110, 36);

    m_addBtn = new QPushButton(tr("Add Rule"));
    m_addBtn->setObjectName("PrimaryActionBtn");
    m_addBtn->setCursor(Qt::PointingHandCursor);
    m_addBtn->setMinimumSize(110, 36);

    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    headerLayout->addWidget(m_addBtn);
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
    m_searchEdit->setPlaceholderText(tr("Search rules or proxies..."));
    m_searchEdit->setClearButtonEnabled(true);

    m_typeFilter = new MenuComboBox;
    m_typeFilter->setObjectName("FilterSelect");

    m_proxyFilter = new MenuComboBox;
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
    m_gridLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_scrollArea->setWidget(m_gridContainer);

    m_emptyState = new QFrame;
    m_emptyState->setObjectName("EmptyState");
    QVBoxLayout *emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    emptyLayout->setSpacing(10);
    emptyLayout->setAlignment(Qt::AlignCenter);

    QLabel *emptyIcon = new QLabel(tr("Search"));
    emptyIcon->setObjectName("EmptyIcon");
    emptyIcon->setAlignment(Qt::AlignCenter);

    m_emptyTitle = new QLabel(tr("No rules yet"));
    m_emptyTitle->setObjectName("EmptyTitle");
    m_emptyTitle->setAlignment(Qt::AlignCenter);

    m_emptyAction = new QPushButton(tr("Fetch Rules"));
    m_emptyAction->setObjectName("EmptyActionBtn");
    m_emptyAction->setCursor(Qt::PointingHandCursor);
    m_emptyAction->setMinimumSize(110, 36);

    emptyLayout->addWidget(emptyIcon);
    emptyLayout->addWidget(m_emptyTitle);
    emptyLayout->addWidget(m_emptyAction, 0, Qt::AlignCenter);

    mainLayout->addWidget(m_scrollArea, 1);
    mainLayout->addWidget(m_emptyState, 1);

    connect(m_refreshBtn, &QPushButton::clicked, this, &RulesView::onRefreshClicked);
    connect(m_addBtn, &QPushButton::clicked, this, &RulesView::onAddRuleClicked);
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
            const QString source = rule.value("source").toString().toLower();
            if (source == "user" || source == "custom") {
                data.isCustom = true;
            } else {
                data.isCustom = RuleUtils::isCustomPayload(data.payload);
            }
            m_rules.push_back(data);
        }

        std::stable_sort(m_rules.begin(), m_rules.end(), [](const RuleItem &a, const RuleItem &b) {
            if (a.isCustom != b.isCustom) return a.isCustom && !b.isCustom;
            return false;
        });

        m_loading = false;
        m_refreshBtn->setEnabled(true);
        m_refreshBtn->setText(tr("Fetch Rules"));
        updateFilterOptions();
        applyFilters();
    });
}

void RulesView::refresh()
{
    if (!m_proxyService) return;
    m_loading = true;
    m_refreshBtn->setEnabled(false);
    m_refreshBtn->setText(tr("Loading..."));
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

void RulesView::onAddRuleClicked()
{
    QString error;
    const QStringList outboundTags = RuleConfigService::loadOutboundTags(QString(), &error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Add Rule"), error);
        return;
    }

    RuleEditorDialog dialog(RuleEditorDialog::Mode::Add, this);
    dialog.setOutboundTags(outboundTags);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    RuleConfigService::RuleEditData data = dialog.editData();
    RuleItem added;
    if (!RuleConfigService::addRule(data, &added, &error)) {
        QMessageBox::warning(this, tr("Add Rule"), error);
        return;
    }

    m_rules.push_back(added);
    updateFilterOptions();
    applyFilters();

    QMessageBox::information(this, tr("Add Rule"),
                             tr("Rules written to route.rules.\nRestart kernel or app to apply."));
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
        const QString typeLabel = RuleUtils::displayRuleTypeLabel(rule.type);
        const bool matchSearch = query.isEmpty()
            || rule.payload.contains(query, Qt::CaseInsensitive)
            || rule.proxy.contains(query, Qt::CaseInsensitive)
            || typeLabel.contains(query, Qt::CaseInsensitive);
        const bool matchType = typeValue.isEmpty()
            || (typeValue == "custom" && rule.isCustom)
            || (typeValue == "default" && !rule.isCustom)
            || (typeValue != "custom" && typeValue != "default"
                && RuleUtils::normalizeRuleTypeKey(rule.type) == typeValue);
        const bool matchProxy = proxyValue.isEmpty()
            || RuleUtils::normalizeProxyValue(rule.proxy) == proxyValue;

        if (matchSearch && matchType && matchProxy) {
            m_filteredRules.push_back(rule);
        }
    }

    std::stable_sort(m_filteredRules.begin(), m_filteredRules.end(), [](const RuleItem &a, const RuleItem &b) {
        if (a.isCustom != b.isCustom) return a.isCustom && !b.isCustom;
        return false;
    });

    rebuildGrid();
    if (m_scrollArea) {
        if (auto *vbar = m_scrollArea->verticalScrollBar()) vbar->setValue(0);
        if (auto *hbar = m_scrollArea->horizontalScrollBar()) hbar->setValue(0);
    }
    updateEmptyState();
}

void RulesView::updateFilterOptions()
{
    const QString currentType = m_typeFilter->currentData().toString();
    const QString currentProxy = m_proxyFilter->currentData().toString();

    QMap<QString, QString> types;
    QSet<QString> proxies;
    bool hasCustom = false;
    bool hasDefault = false;
    for (const auto &rule : std::as_const(m_rules)) {
        const QString typeKey = RuleUtils::normalizeRuleTypeKey(rule.type);
        if (!rule.isCustom) {
            hasDefault = true;
            if (typeKey != "default" && !types.contains(typeKey)) {
                types.insert(typeKey, RuleUtils::displayRuleTypeLabel(rule.type));
            }
        }
        proxies.insert(RuleUtils::normalizeProxyValue(rule.proxy));
        if (rule.isCustom) hasCustom = true;
    }

    m_typeFilter->blockSignals(true);
    m_typeFilter->clear();
    m_typeFilter->addItem(tr("Type"), QString());
    if (hasCustom) {
        m_typeFilter->addItem(tr("Custom"), "custom");
    }
    if (hasDefault) {
        m_typeFilter->addItem(tr("Default"), "default");
    }
    QStringList typeKeys = types.keys();
    typeKeys.sort();
    for (const auto &key : typeKeys) {
        const QString label = types.value(key);
        m_typeFilter->addItem(label, key);
    }
    int typeIndex = m_typeFilter->findData(currentType);
    m_typeFilter->setCurrentIndex(typeIndex < 0 ? 0 : typeIndex);
    m_typeFilter->blockSignals(false);

    m_proxyFilter->blockSignals(true);
    m_proxyFilter->clear();
    m_proxyFilter->addItem(tr("Target Proxy"), QString());
    if (proxies.contains("direct")) {
        m_proxyFilter->addItem(tr("Direct"), "direct");
    }
    if (proxies.contains("reject")) {
        m_proxyFilter->addItem(tr("Reject"), "reject");
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
    if (!m_gridLayout || !m_scrollArea || !m_gridContainer) return;

    m_gridContainer->setUpdatesEnabled(false);
    m_scrollArea->viewport()->setUpdatesEnabled(false);

    while (m_gridLayout->count() > 0) {
        QLayoutItem *item = m_gridLayout->takeAt(0);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    const int availableWidth = m_scrollArea->viewport()->width();
    const int spacing = m_gridLayout->spacing();
    const int minColumns = 2;
    const int maxColumns = 4;
    const int idealCardWidth = 320;
    int columns = availableWidth / idealCardWidth;
    columns = qMax(minColumns, qMin(columns, maxColumns));
    m_columnCount = columns;
    const int totalSpacing = spacing * (columns - 1);
    const int cardWidth = qMax(0, (availableWidth - totalSpacing) / columns);
    const int cardHeight = qMax(150, qRound(cardWidth * 0.55));

    int row = 0;
    int col = 0;
    for (int i = 0; i < m_filteredRules.size(); ++i) {
        RuleCard *card = new RuleCard(m_filteredRules[i], i + 1, m_gridContainer);
        connect(card, &RuleCard::editRequested, this, &RulesView::handleEditRule);
        connect(card, &RuleCard::deleteRequested, this, &RulesView::handleDeleteRule);
        card->setFixedSize(cardWidth, cardHeight);
        m_gridLayout->addWidget(card, row, col);
        col++;
        if (col >= columns) {
            col = 0;
            row++;
        }
    }
    for (int i = 0; i < columns; ++i) {
        m_gridLayout->setColumnStretch(i, 1);
    }

    m_gridContainer->setUpdatesEnabled(true);
    m_scrollArea->viewport()->setUpdatesEnabled(true);
    m_gridContainer->update();
}

void RulesView::updateEmptyState()
{
    const bool hasFilters = !m_searchEdit->text().trimmed().isEmpty()
        || !m_typeFilter->currentData().toString().isEmpty()
        || !m_proxyFilter->currentData().toString().isEmpty();

    if (m_filteredRules.isEmpty()) {
        m_emptyState->show();
        m_scrollArea->hide();
        m_emptyTitle->setText(hasFilters ? tr("No matching rules") : tr("No rules yet"));
        m_emptyAction->setText(hasFilters ? tr("Clear Filters") : tr("Fetch Rules"));
    } else {
        m_emptyState->hide();
        m_scrollArea->show();
    }
}

void RulesView::handleEditRule(const RuleItem &rule)
{
    QString error;
    const QString outbound = RuleUtils::normalizeProxyValue(rule.proxy);
    const QStringList outboundTags = RuleConfigService::loadOutboundTags(outbound, &error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Edit Match Type"), error);
        return;
    }

    RuleEditorDialog dialog(RuleEditorDialog::Mode::Edit, this);
    dialog.setOutboundTags(outboundTags);
    dialog.setRuleSetName(RuleConfigService::findRuleSet(rule));
    if (!dialog.setEditRule(rule, &error)) {
        QMessageBox::warning(this, tr("Edit Match Type"), error);
        return;
    }
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    RuleConfigService::RuleEditData data = dialog.editData();
    RuleItem updated;
    if (!RuleConfigService::updateRule(rule, data, &updated, &error)) {
        QMessageBox::warning(this, tr("Edit Match Type"), error);
        return;
    }

    for (auto &r : m_rules) {
        if (r.payload == rule.payload && r.proxy == rule.proxy && r.type == rule.type) {
            r = updated;
            break;
        }
    }
    updateFilterOptions();
    applyFilters();
}

void RulesView::handleDeleteRule(const RuleItem &rule)
{
    const QMessageBox::StandardButton btn = QMessageBox::question(
        this, tr("Delete Rule"), tr("Delete this custom rule?"));
    if (btn != QMessageBox::Yes) return;

    QString error;
    if (!RuleConfigService::removeRule(rule, &error)) {
        QMessageBox::warning(this, tr("Delete Rule"), error);
        return;
    }

    auto it = std::remove_if(m_rules.begin(), m_rules.end(), [&rule](const RuleItem &r) {
        return r.payload == rule.payload && r.proxy == rule.proxy && r.type == rule.type;
    });
    m_rules.erase(it, m_rules.end());
    updateFilterOptions();
    applyFilters();
}

void RulesView::updateStyle()
{
    ThemeManager &tm = ThemeManager::instance();
    setStyleSheet(tm.loadStyleSheet(":/styles/rules_view.qss"));
}

void RulesView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_filteredRules.isEmpty()) {
        const int availableWidth = m_scrollArea->viewport()->width();
        const int spacing = m_gridLayout->spacing();
        const int minColumns = 2;
        const int maxColumns = 4;
        const int idealCardWidth = 320;
        int columns = availableWidth / idealCardWidth;
        columns = qMax(minColumns, qMin(columns, maxColumns));
        if (columns != m_columnCount) {
            rebuildGrid();
            return;
        }

        const int totalSpacing = spacing * (columns - 1);
        const int cardWidth = qMax(0, (availableWidth - totalSpacing) / columns);
        const int cardHeight = qMax(150, qRound(cardWidth * 0.55));
        for (int i = 0; i < m_gridLayout->count(); ++i) {
            if (QLayoutItem *item = m_gridLayout->itemAt(i)) {
                if (QWidget *widget = item->widget()) {
                    widget->setFixedSize(cardWidth, cardHeight);
                }
            }
        }
    }
}

