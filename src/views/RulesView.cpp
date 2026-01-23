#include "RulesView.h"
#include "core/ProxyService.h"
#include "utils/ThemeManager.h"
#include "storage/ConfigManager.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFrame>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
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
#include <QtMath>
#include <QStringList>
#include <QVBoxLayout>
#include <utility>

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

void RulesView::onAddRuleClicked()
{
    struct RuleFieldInfo {
        QString label;
        QString key;
        QString placeholder;
        bool numeric = false;
    };
    const QList<RuleFieldInfo> fields = {
        {tr("域名"), "domain", tr("示例: example.com")},
        {tr("域名后缀"), "domain_suffix", tr("示例: example.com")},
        {tr("域名关键字"), "domain_keyword", tr("示例: google")},
        {tr("域名正则"), "domain_regex", tr("示例: ^.*\\\\.example\\\\.com$")},
        {tr("IP CIDR"), "ip_cidr", tr("示例: 192.168.0.0/16")},
        {tr("源 IP CIDR"), "source_ip_cidr", tr("示例: 10.0.0.0/8")},
        {tr("端口"), "port", tr("示例: 80,443"), true},
        {tr("源端口"), "source_port", tr("示例: 12345"), true},
        {tr("端口范围"), "port_range", tr("示例: 10000:20000")},
        {tr("源端口范围"), "source_port_range", tr("示例: 10000:20000")},
        {tr("进程名"), "process_name", tr("示例: chrome.exe")},
        {tr("进程路径"), "process_path", tr("示例: C:\\\\Program Files\\\\App\\\\app.exe")},
        {tr("进程路径正则"), "process_path_regex", tr("示例: ^C:\\\\\\\\Program Files\\\\\\\\.+")},
        {tr("包名"), "package_name", tr("示例: com.example.app")},
    };

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
    const QString configPath = ConfigManager::instance().getActiveConfigPath();
    const QJsonObject config = ConfigManager::instance().loadConfig(configPath);
    if (!config.isEmpty() && config.value("outbounds").isArray()) {
        const QJsonArray outbounds = config.value("outbounds").toArray();
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

    QLabel *hintLabel = new QLabel(tr("说明：规则将写入本地 rule-set 文件，并自动引用。文件修改后会自动重载。"), &dialog);
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

    QJsonObject headlessRule;
    headlessRule.insert(field.key, payload);

    const QString outboundTag = outboundCombo->currentText().trimmed();
    if (outboundTag.isEmpty()) {
        QMessageBox::warning(this, tr("添加规则"), tr("出口(outbound)不能为空。"));
        return;
    }
    QString ruleSetTag = outboundTag;
    ruleSetTag.replace(QRegularExpression("[^A-Za-z0-9_.-]"), "_");
    if (ruleSetTag.isEmpty()) {
        ruleSetTag = "default";
    }
    ruleSetTag = QString("user-%1").arg(ruleSetTag);

    const QString ruleSetFile = ConfigManager::instance().getConfigDir()
        + QString("/rule_set_%1.json").arg(ruleSetTag);

    QJsonObject ruleSetJson;
    QJsonArray ruleSetRules;
    QFile rsFile(ruleSetFile);
    if (rsFile.exists()) {
        if (rsFile.open(QIODevice::ReadOnly)) {
            QJsonDocument rsDoc = QJsonDocument::fromJson(rsFile.readAll());
            rsFile.close();
            if (rsDoc.isObject()) {
                ruleSetJson = rsDoc.object();
                if (ruleSetJson.value("rules").isArray()) {
                    ruleSetRules = ruleSetJson.value("rules").toArray();
                }
            }
        }
    }
    ruleSetRules.append(headlessRule);
    if (!ruleSetJson.contains("version")) {
        ruleSetJson["version"] = 2;
    }
    ruleSetJson["rules"] = ruleSetRules;

    if (!rsFile.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("添加规则"), tr("无法写入规则集文件：%1").arg(ruleSetFile));
        return;
    }
    rsFile.write(QJsonDocument(ruleSetJson).toJson(QJsonDocument::Indented));
    rsFile.close();

    QJsonObject newConfig = ConfigManager::instance().loadConfig(configPath);
    if (newConfig.isEmpty()) {
        QMessageBox::warning(this, tr("添加规则"), tr("无法读取配置文件：%1").arg(configPath));
        return;
    }

    QJsonObject route = newConfig.value("route").toObject();

    QJsonArray ruleSets = route.value("rule_set").toArray();
    bool hasRuleSet = false;
    for (const auto &rsVal : ruleSets) {
        if (!rsVal.isObject()) continue;
        const QJsonObject rs = rsVal.toObject();
        if (rs.value("tag").toString() == ruleSetTag) {
            hasRuleSet = true;
            break;
        }
    }
    if (!hasRuleSet) {
        QJsonObject rs;
        rs["type"] = "local";
        rs["tag"] = ruleSetTag;
        rs["format"] = "source";
        rs["path"] = ruleSetFile;
        ruleSets.append(rs);
        route["rule_set"] = ruleSets;
    }

    QJsonArray rules = route.value("rules").toArray();
    bool hasRouteRule = false;
    for (const auto &ruleVal : rules) {
        if (!ruleVal.isObject()) continue;
        const QJsonObject ruleObj = ruleVal.toObject();
        if (ruleObj.value("rule_set").toString() == ruleSetTag
            && ruleObj.value("outbound").toString() == outboundTag) {
            hasRouteRule = true;
            break;
        }
    }
    if (!hasRouteRule) {
        QJsonObject routeRule;
        routeRule["rule_set"] = ruleSetTag;
        routeRule["action"] = "route";
        routeRule["outbound"] = outboundTag;
        rules.append(routeRule);
        route["rules"] = rules;
    }

    newConfig["route"] = route;

    if (!ConfigManager::instance().saveConfig(configPath, newConfig)) {
        QMessageBox::warning(this, tr("添加规则"), tr("保存配置失败：%1").arg(configPath));
        return;
    }

    QMessageBox::information(this, tr("添加规则"),
                             tr("规则已写入本地 rule-set 文件并自动引用。\n文件修改后会自动重载。"));
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
            padding: 6px 10px;
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
