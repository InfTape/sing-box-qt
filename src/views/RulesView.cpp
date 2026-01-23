#include "RulesView.h"
#include "core/ProxyService.h"
#include "utils/ThemeManager.h"
#include "storage/ConfigManager.h"
#include "storage/DatabaseService.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QCursor>
#include <QtMath>
#include <QStringList>
#include <QVBoxLayout>
#include <utility>
#include <algorithm>

class RulesView;

namespace {
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

struct RuleFieldInfo {
    QString label;
    QString key;
    QString placeholder;
    bool numeric = false;
};

inline QList<RuleFieldInfo> ruleFieldInfos()
{
    return {
        {QObject::tr("域名"), "domain", QObject::tr("示例: example.com")},
        {QObject::tr("域名后缀"), "domain_suffix", QObject::tr("示例: example.com")},
        {QObject::tr("域名关键字"), "domain_keyword", QObject::tr("示例: google")},
        {QObject::tr("域名正则"), "domain_regex", QObject::tr("示例: ^.*\\\\.example\\\\.com$")},
        {QObject::tr("IP CIDR"), "ip_cidr", QObject::tr("示例: 192.168.0.0/16")},
        {QObject::tr("私有 IP"), "ip_is_private", QObject::tr("示例: true / false")},
        {QObject::tr("源 IP CIDR"), "source_ip_cidr", QObject::tr("示例: 10.0.0.0/8")},
        {QObject::tr("端口"), "port", QObject::tr("示例: 80,443"), true},
        {QObject::tr("源端口"), "source_port", QObject::tr("示例: 80,443"), true},
        {QObject::tr("端口范围"), "port_range", QObject::tr("示例: 10000:20000")},
        {QObject::tr("源端口范围"), "source_port_range", QObject::tr("示例: 10000:20000")},
        {QObject::tr("进程名"), "process_name", QObject::tr("示例: chrome.exe")},
        {QObject::tr("进程路径"), "process_path", QObject::tr("示例: C:\\\\Program Files\\\\App\\\\app.exe")},
        {QObject::tr("进程路径正则"), "process_path_regex", QObject::tr("示例: ^C:\\\\\\\\Program Files\\\\\\\\.+")},
        {QObject::tr("包名"), "package_name", QObject::tr("示例: com.example.app")},
    };
}

} // namespace

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

    m_addBtn = new QPushButton(tr("添加规则"));
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
    m_searchEdit->setPlaceholderText(tr("搜索规则内容或代理..."));
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
                data.isCustom = isCustomPayload(data.payload);
            }
            m_rules.push_back(data);
        }

        std::stable_sort(m_rules.begin(), m_rules.end(), [](const RuleItem &a, const RuleItem &b) {
            if (a.isCustom != b.isCustom) return a.isCustom && !b.isCustom;
            return false;
        });

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

void RulesView::onAddRuleClicked()
{
    const QList<RuleFieldInfo> fields = ruleFieldInfos();

    QDialog dialog(this);
    dialog.setWindowTitle(tr("添加规则"));
    dialog.setModal(true);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QFormLayout *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setFormAlignment(Qt::AlignLeft);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);

    MenuComboBox *typeCombo = new MenuComboBox(&dialog);
    for (const auto &field : fields) {
        typeCombo->addItem(field.label, field.key);
    }

    QPlainTextEdit *valueEdit = new QPlainTextEdit(&dialog);
    valueEdit->setPlaceholderText(fields.first().placeholder + tr("（可用逗号或换行分隔）"));
    valueEdit->setFixedHeight(80);

    MenuComboBox *outboundCombo = new MenuComboBox(&dialog);
    QSet<QString> outboundTags;
    const QString configPath = activeConfigPath();
    const QJsonObject configOut = ConfigManager::instance().loadConfig(configPath);
    if (!configOut.isEmpty() && configOut.value("outbounds").isArray()) {
        const QJsonArray outbounds = configOut.value("outbounds").toArray();
        for (const auto &item : outbounds) {
            if (!item.isObject()) continue;
            const QString tag = item.toObject().value("tag").toString().trimmed();
            if (!tag.isEmpty()) outboundTags.insert(tag);
        }
    }
    if (outboundTags.isEmpty()) {
        outboundTags.insert("direct");
    }
    QStringList outboundList = outboundTags.values();
    outboundList.sort();
    outboundCombo->addItems(outboundList);

    form->addRow(tr("匹配类型:"), typeCombo);
    form->addRow(tr("匹配内容:"), valueEdit);
    form->addRow(tr("出口(outbound):"), outboundCombo);

    QLabel *hintLabel = new QLabel(tr("说明：规则将写入 route.rules（1.11+ 格式）。修改后需重启内核生效。"), &dialog);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("color: #94a3b8; font-size: 12px;");

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("取消"));

    layout->addLayout(form);
    layout->addWidget(hintLabel);
    layout->addWidget(buttons);

    QObject::connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, [=](int index) {
        if (index >= 0 && index < fields.size()) {
            valueEdit->setPlaceholderText(fields[index].placeholder + tr("（可用逗号或换行分隔）"));
        }
    });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) return;

    const int fieldIndex = typeCombo->currentIndex();
    if (fieldIndex < 0 || fieldIndex >= fields.size()) return;
    const RuleFieldInfo field = fields[fieldIndex];

    const QString rawText = valueEdit->toPlainText().trimmed();
    if (rawText.isEmpty()) {
        QMessageBox::warning(this, tr("添加规则"), tr("匹配内容不能为空。"));
        return;
    }

    QStringList values = rawText.split(QRegularExpression("[,\\n]"), Qt::SkipEmptyParts);
    for (QString &v : values) v = v.trimmed();
    values.removeAll(QString());
    if (values.isEmpty()) {
        QMessageBox::warning(this, tr("添加规则"), tr("匹配内容不能为空。"));
        return;
    }

    QJsonArray payload;
    if (field.numeric) {
        for (const auto &v : values) {
            bool ok = false;
            int num = v.toInt(&ok);
            if (!ok) {
                QMessageBox::warning(this, tr("添加规则"), tr("端口必须为数字：%1").arg(v));
                return;
            }
            payload.append(num);
        }
    } else {
        for (const auto &v : values) {
            payload.append(v);
        }
    }

    const QString outboundTag = outboundCombo->currentText().trimmed();
    if (outboundTag.isEmpty()) {
        QMessageBox::warning(this, tr("添加规则"), tr("出口(outbound)不能为空。"));
        return;
    }

    QJsonObject newConfig = ConfigManager::instance().loadConfig(configPath);
    if (newConfig.isEmpty()) {
        QMessageBox::warning(this, tr("添加规则"), tr("无法读取配置文件：%1").arg(configPath));
        return;
    }

    QJsonObject route = newConfig.value("route").toObject();

    QJsonObject routeRule;
    if (field.key == "ip_is_private") {
        if (payload.size() != 1) {
            QMessageBox::warning(this, tr("添加规则"), tr("ip_is_private 只能设置一个值（true/false）。"));
            return;
        }
        const QString raw = payload.first().toString().toLower();
        if (raw != "true" && raw != "false") {
            QMessageBox::warning(this, tr("添加规则"), tr("ip_is_private 只能为 true 或 false。"));
            return;
        }
        routeRule.insert(field.key, raw == "true");
    } else if (payload.size() == 1) {
        routeRule.insert(field.key, payload.first());
    } else {
        routeRule.insert(field.key, payload);
    }
    routeRule["action"] = "route";
    routeRule["outbound"] = outboundTag;

    QJsonArray rules = route.value("rules").toArray();
    bool hasRouteRule = false;
    int existingRuleIndex = -1;
    for (int i = 0; i < rules.size(); ++i) {
        if (!rules[i].isObject()) continue;
        const QJsonObject ruleObj = rules[i].toObject();
        if (ruleObj == routeRule) {
            hasRouteRule = true;
            existingRuleIndex = i;
            break;
        }
    }
    int insertIndex = rules.size();
    const int directIndex = [&rules]() {
        for (int i = 0; i < rules.size(); ++i) {
            if (!rules[i].isObject()) continue;
            const QJsonObject ruleObj = rules[i].toObject();
            if (ruleObj.value("clash_mode").toString() == "direct") {
                return i;
            }
        }
        return -1;
    }();
    const int globalIndex = [&rules]() {
        for (int i = 0; i < rules.size(); ++i) {
            if (!rules[i].isObject()) continue;
            const QJsonObject ruleObj = rules[i].toObject();
            if (ruleObj.value("clash_mode").toString() == "global") {
                return i;
            }
        }
        return -1;
    }();
    int insertionIndex = -1;
    if (directIndex >= 0 && globalIndex >= 0) {
        insertionIndex = qMax(directIndex, globalIndex);
    } else if (directIndex >= 0) {
        insertionIndex = directIndex;
    } else if (globalIndex >= 0) {
        insertionIndex = globalIndex;
    }
    if (insertionIndex >= 0) {
        insertIndex = insertionIndex + 1;
    } else if (!rules.isEmpty()) {
        insertIndex = 0;
    }

    if (!hasRouteRule) {
        rules.insert(insertIndex, routeRule);
    } else if (existingRuleIndex >= 0 && existingRuleIndex > insertIndex) {
        QJsonObject existingRule = rules[existingRuleIndex].toObject();
        rules.removeAt(existingRuleIndex);
        rules.insert(insertIndex, existingRule);
    }
    route["rules"] = rules;

    newConfig["route"] = route;

    if (!ConfigManager::instance().saveConfig(configPath, newConfig)) {
        QMessageBox::warning(this, tr("添加规则"), tr("保存配置失败：%1").arg(configPath));
        return;
    }

    QMessageBox::information(this, tr("添加规则"),
                             tr("规则已写入 route.rules。\n请重启内核或应用以生效。"));
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
        const bool matchType = typeValue.isEmpty()
            || (typeValue == "custom" && rule.isCustom)
            || (typeValue != "custom" && rule.type == typeValue);
        const bool matchProxy = proxyValue.isEmpty()
            || normalizeProxyValue(rule.proxy) == proxyValue;

        if (matchSearch && matchType && matchProxy) {
            m_filteredRules.push_back(rule);
        }
    }

    std::stable_sort(m_filteredRules.begin(), m_filteredRules.end(), [](const RuleItem &a, const RuleItem &b) {
        if (a.isCustom != b.isCustom) return a.isCustom && !b.isCustom;
        return false;
    });

    rebuildGrid();
    updateEmptyState();
}

void RulesView::updateFilterOptions()
{
    const QString currentType = m_typeFilter->currentData().toString();
    const QString currentProxy = m_proxyFilter->currentData().toString();

    QSet<QString> types;
    QSet<QString> proxies;
    bool hasCustom = false;
    for (const auto &rule : std::as_const(m_rules)) {
        types.insert(rule.type);
        proxies.insert(normalizeProxyValue(rule.proxy));
        if (rule.isCustom) hasCustom = true;
    }

    m_typeFilter->blockSignals(true);
    m_typeFilter->clear();
    m_typeFilter->addItem(tr("类型"), QString());
    if (hasCustom) {
        m_typeFilter->addItem(tr("自定义"), "custom");
    }
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
        QWidget *card = createRuleCard(m_filteredRules[i], i + 1);
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

    QLabel *typeTag = new QLabel(rule.isCustom ? tr("自定义规则") : rule.type);
    typeTag->setObjectName("RuleTag");
    typeTag->setProperty("tagType", ruleTagType(rule));

    QLabel *indexLabel = new QLabel(QString("#%1").arg(index));
    indexLabel->setObjectName("RuleIndex");

    headerLayout->addWidget(typeTag);
    headerLayout->addStretch();
    if (rule.isCustom) {
        QPushButton *menuBtn = new QPushButton("...");
        menuBtn->setFlat(true);
        menuBtn->setCursor(Qt::PointingHandCursor);
        menuBtn->setFixedSize(28, 24);
        menuBtn->setObjectName("RuleMenuBtn");

        RoundedMenu *menu = new RoundedMenu(card);
        menu->setObjectName("RuleMenu");
        ThemeManager &tm = ThemeManager::instance();
        menu->setThemeColors(tm.getColor("bg-secondary"), tm.getColor("primary"));
        connect(&tm, &ThemeManager::themeChanged, menu, [menu, &tm]() {
            menu->setThemeColors(tm.getColor("bg-secondary"), tm.getColor("primary"));
        });

        QAction *editTypeAct = menu->addAction(tr("修改匹配类型"));
        QAction *removeAct = menu->addAction(tr("删除规则"));
        removeAct->setObjectName("DeleteAction");

        connect(menuBtn, &QPushButton::clicked, [menuBtn, menu]() {
            menu->exec(menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
        });
        connect(editTypeAct, &QAction::triggered, card, [this, rule]() {
            changeRuleType(rule);
        });
        connect(removeAct, &QAction::triggered, card, [this, rule]() {
            const QMessageBox::StandardButton btn = QMessageBox::question(
                this, tr("删除规则"), tr("确定删除该自定义规则吗？"));
            if (btn != QMessageBox::Yes) return;
            if (!removeRuleFromConfig(rule)) {
                QMessageBox::warning(this, tr("删除规则"), tr("删除失败，未找到匹配规则。"));
                return;
            }
            auto it = std::remove_if(m_rules.begin(), m_rules.end(), [&rule](const RuleItem &r) {
                return r.payload == rule.payload && r.proxy == rule.proxy && r.type == rule.type;
            });
            m_rules.erase(it, m_rules.end());
            applyFilters();
        });
        headerLayout->addWidget(menuBtn);
    }
    headerLayout->addWidget(indexLabel);

    QVBoxLayout *bodyLayout = new QVBoxLayout;
    bodyLayout->setSpacing(6);

    QLabel *contentValue = new QLabel(rule.payload);
    contentValue->setObjectName("RuleValue");
    contentValue->setWordWrap(true);

    QLabel *proxyValue = new QLabel(displayProxyLabel(rule.proxy));
    proxyValue->setObjectName("RuleProxyTag");
    proxyValue->setProperty("tagType", proxyTagType(rule.proxy));

    bodyLayout->addWidget(contentValue);
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
    if (value.startsWith("route(") && value.endsWith(")")) {
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

QString RulesView::ruleTagType(const RuleItem &rule) const
{
    if (rule.isCustom) return "info";
    const QString lower = rule.type.toLower();
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

bool RulesView::isCustomPayload(const QString &payload) const
{
    const QString p = payload.toLower();
    return p.startsWith("domain")
        || p.startsWith("ip")
        || p.startsWith("process")
        || p.startsWith("package")
        || p.startsWith("port")
        || p.startsWith("source");
}

QString RulesView::activeConfigPath() const
{
    const QString subPath = DatabaseService::instance().getActiveConfigPath();
    if (!subPath.isEmpty()) return subPath;
    return ConfigManager::instance().getActiveConfigPath();
}

bool RulesView::removeRuleFromConfig(const RuleItem &rule)
{
    const QString path = activeConfigPath();
    if (path.isEmpty()) return false;

    QJsonObject config = ConfigManager::instance().loadConfig(path);
    if (config.isEmpty()) return false;

    QJsonObject route = config.value("route").toObject();
    QJsonArray rules = route.value("rules").toArray();

    const QString payload = rule.payload.trimmed();
    const int eq = payload.indexOf('=');
    if (eq <= 0) return false;
    const QString key = payload.left(eq).trimmed();
    const QString valueStr = payload.mid(eq + 1).trimmed();
    QStringList values = valueStr.split(',', Qt::SkipEmptyParts);
    for (QString &v : values) v = v.trimmed();

    auto matches = [&](const QJsonObject &obj) -> bool {
        if (obj.contains("action") && obj.value("action").toString() != "route") return false;
        if (!obj.contains(key)) return false;
        const QString objOutbound = normalizeProxyValue(obj.value("outbound").toString());
        if (normalizeProxyValue(rule.proxy) != objOutbound) return false;
        const QJsonValue v = obj.value(key);
        if (v.isArray()) {
            QStringList arr;
            for (const auto &it : v.toArray()) {
                if (it.isDouble()) arr << QString::number(it.toInt());
                else arr << it.toString();
            }
            arr.sort();
            QStringList sortedValues = values;
            sortedValues.sort();
            return arr == sortedValues;
        }
        if (v.isDouble()) {
            return values.size() == 1 && v.toInt() == values.first().toInt();
        }
        if (v.isBool()) {
            return values.size() == 1
                && ((v.toBool() && values.first().toLower() == "true")
                    || (!v.toBool() && values.first().toLower() == "false"));
        }
        return values.size() == 1 && v.toString().trimmed() == values.first();
    };

    bool removed = false;
    for (int i = 0; i < rules.size(); ++i) {
        if (!rules[i].isObject()) continue;
        if (matches(rules[i].toObject())) {
            rules.removeAt(i);
            removed = true;
            break;
        }
    }

    if (!removed) return false;

    route["rules"] = rules;
    config["route"] = route;
    if (!ConfigManager::instance().saveConfig(path, config)) {
        return false;
    }
    return true;
}

bool RulesView::changeRuleType(const RuleItem &rule)
{
    const QList<RuleFieldInfo> fields = ruleFieldInfos();

    // 拆分当前 payload
    const QString payload = rule.payload.trimmed();
    const int eq = payload.indexOf('=');
    if (eq <= 0) {
        QMessageBox::warning(this, tr("修改类型"), tr("无法解析当前规则内容。"));
        return false;
    }
    const QString currentKey = payload.left(eq).trimmed();
    const QString currentValue = payload.mid(eq + 1).trimmed();

    QDialog dialog(this);
    dialog.setWindowTitle(tr("修改匹配类型"));
    dialog.setModal(true);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QFormLayout *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);

    MenuComboBox *typeCombo = new MenuComboBox(&dialog);
    int currentIndex = 0;
    for (int i = 0; i < fields.size(); ++i) {
        typeCombo->addItem(fields[i].label, fields[i].key);
        if (fields[i].key == currentKey) currentIndex = i;
    }
    typeCombo->setCurrentIndex(currentIndex);

    QPlainTextEdit *valueEdit = new QPlainTextEdit(&dialog);
    valueEdit->setPlainText(currentValue);
    valueEdit->setFixedHeight(80);

    // 出口选择
    MenuComboBox *outboundCombo = new MenuComboBox(&dialog);
    QSet<QString> outboundTags;
    const QString configPath = activeConfigPath();
    const QJsonObject config = ConfigManager::instance().loadConfig(configPath);
    if (!config.isEmpty() && config.value("outbounds").isArray()) {
        const QJsonArray outbounds = config.value("outbounds").toArray();
        for (const auto &item : outbounds) {
            if (!item.isObject()) continue;
            const QString tag = item.toObject().value("tag").toString().trimmed();
            if (!tag.isEmpty()) outboundTags.insert(tag);
        }
    }
    const QString currentOutbound = normalizeProxyValue(rule.proxy);
    if (!outboundTags.contains(currentOutbound) && !currentOutbound.isEmpty()) {
        outboundTags.insert(currentOutbound);
    }
    if (outboundTags.isEmpty()) outboundTags.insert("direct");
    QStringList outboundList = outboundTags.values();
    outboundList.sort();
    outboundCombo->addItems(outboundList);
    int outboundIndex = outboundCombo->findText(currentOutbound);
    if (outboundIndex >= 0) outboundCombo->setCurrentIndex(outboundIndex);

    form->addRow(tr("匹配类型:"), typeCombo);
    form->addRow(tr("匹配内容:"), valueEdit);
    form->addRow(tr("出口(outbound):"), outboundCombo);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("取消"));

    layout->addLayout(form);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) return false;

    const int idx = typeCombo->currentIndex();
    if (idx < 0 || idx >= fields.size()) return false;
    const RuleFieldInfo field = fields[idx];

    QString rawText = valueEdit->toPlainText().trimmed();
    if (rawText.isEmpty()) {
        QMessageBox::warning(this, tr("修改类型"), tr("匹配内容不能为空。"));
        return false;
    }

    QStringList values = rawText.split(QRegularExpression("[,\\n]"), Qt::SkipEmptyParts);
    for (QString &v : values) v = v.trimmed();
    values.removeAll(QString());
    if (values.isEmpty()) {
        QMessageBox::warning(this, tr("修改类型"), tr("匹配内容不能为空。"));
        return false;
    }

    QJsonObject routeRule;
    if (field.numeric) {
        QJsonArray arr;
        for (const auto &v : values) {
            bool ok = false;
            int num = v.toInt(&ok);
            if (!ok) {
                QMessageBox::warning(this, tr("修改类型"), tr("端口必须为数字：%1").arg(v));
                return false;
            }
            arr.append(num);
        }
        if (arr.size() == 1) {
            routeRule.insert(field.key, arr.first());
        } else {
            routeRule.insert(field.key, arr);
        }
    } else if (field.key == "ip_is_private") {
        if (values.size() != 1) {
            QMessageBox::warning(this, tr("修改类型"), tr("ip_is_private 只能设置一个值（true/false）。"));
            return false;
        }
        const QString raw = values.first().toLower();
        if (raw != "true" && raw != "false") {
            QMessageBox::warning(this, tr("修改类型"), tr("ip_is_private 只能为 true 或 false。"));
            return false;
        }
        routeRule.insert(field.key, raw == "true");
    } else {
        if (values.size() == 1) {
            routeRule.insert(field.key, values.first());
        } else {
            QJsonArray arr;
            for (const auto &v : values) arr.append(v);
            routeRule.insert(field.key, arr);
        }
    }
    routeRule["action"] = "route";
    routeRule["outbound"] = normalizeProxyValue(outboundCombo->currentText());

    // 先删除旧规则
    if (!removeRuleFromConfig(rule)) {
        QMessageBox::warning(this, tr("修改类型"), tr("未找到需替换的旧规则。"));
        return false;
    }

    const QString path = activeConfigPath();
    QJsonObject configObj = ConfigManager::instance().loadConfig(path);
    if (configObj.isEmpty()) return false;
    QJsonObject route = configObj.value("route").toObject();
    QJsonArray rules = route.value("rules").toArray();

    auto findIndex = [&rules](const QString &mode) -> int {
        for (int i = 0; i < rules.size(); ++i) {
            if (!rules[i].isObject()) continue;
            const QJsonObject obj = rules[i].toObject();
            if (obj.value("clash_mode").toString() == mode) return i;
        }
        return -1;
    };
    int insertIndex = rules.size();
    const int directIndex = findIndex("direct");
    const int globalIndex = findIndex("global");
    if (directIndex >= 0 && globalIndex >= 0) insertIndex = qMax(directIndex, globalIndex) + 1;
    else if (directIndex >= 0) insertIndex = directIndex + 1;
    else if (globalIndex >= 0) insertIndex = globalIndex + 1;
    else if (!rules.isEmpty()) insertIndex = 0;

    rules.insert(insertIndex, routeRule);
    route["rules"] = rules;
    configObj["route"] = route;

    if (!ConfigManager::instance().saveConfig(path, configObj)) {
        QMessageBox::warning(this, tr("修改类型"), tr("保存配置失败"));
        return false;
    }

    // 更新内存列表
    for (auto &r : m_rules) {
        if (r.payload == rule.payload && r.proxy == rule.proxy && r.type == rule.type) {
            r.type = field.key;
            if (values.size() == 1) {
                r.payload = QString("%1=%2").arg(field.key, values.first());
            } else {
                r.payload = QString("%1=%2").arg(field.key, values.join(","));
            }
            r.proxy = normalizeProxyValue(outboundCombo->currentText());
            break;
        }
    }
    applyFilters();
    return true;
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
            border: 1px solid #353b43;
            border-radius: 16px;
        }
        #SearchInput, #FilterSelect {
            background-color: %7;
            border: 1px solid #353b43;
            border-radius: 12px;
            padding: 8px 12px;
            color: %1;
        }
        #SearchInput:focus, #FilterSelect:focus {
            border-color: #353b43;
        }
        #RuleCard {
            background-color: %7;
            border: 1px solid #353b43;
            border-radius: 16px;
        }
        #RuleCard:hover {
            border-color: #353b43;
        }
        #RuleMenuBtn {
            background-color: %5;
            color: %1;
            border: 1px solid %6;
            font-size: 14px;
            font-weight: bold;
            padding: 0 8px;
            border-radius: 10px;
        }
        #RuleMenuBtn:hover {
            background-color: %3;
            color: white;
        }
        #RuleTag {
            padding: 3px 8px;
            border-radius: 8px;
            font-size: 11px;
            font-weight: 600;
            min-width: 0;
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
            padding: 6px 10px;
            border-radius: 10px;
            font-size: 11px;
            font-weight: 600;
        }
        #RuleProxyTag[tagType="success"] { color: #10b981; background: rgba(16,185,129,0.12); }
        #RuleProxyTag[tagType="error"] { color: #ef4444; background: rgba(239,68,68,0.12); }
        #RuleProxyTag[tagType="info"] { color: #3b82f6; background: rgba(59,130,246,0.12); }
        #RuleMenu {
            background: transparent;
            border: none;
            padding: 6px;
        }
        #RuleMenu::panel {
            background: transparent;
            border: none;
        }
        #RuleMenu::item {
            color: %1;
            padding: 8px 14px;
            border-radius: 10px;
        }
        #RuleMenu::indicator {
            width: 14px;
            height: 14px;
        }
        #RuleMenu::indicator:checked {
            image: url(:/icons/check.svg);
        }
        #RuleMenu::indicator:unchecked {
            image: none;
        }
        #RuleMenu::item:selected {
            background-color: %5;
            color: %1;
        }
        #RuleMenu::separator {
            height: 1px;
            background-color: %6;
            margin: 6px 4px;
        }
        #DeleteAction {
            color: #ef4444;
        }
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
