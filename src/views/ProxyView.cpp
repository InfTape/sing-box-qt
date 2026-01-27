#include "ProxyView.h"
#include "core/ProxyService.h"
#include "core/DelayTestService.h"
#include "utils/ThemeManager.h"
#include "storage/AppSettings.h"
#include "widgets/ChevronToggle.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QBrush>
#include <QStyledItemDelegate>
#include <QTreeWidgetItemIterator>
#include <QFrame>
#include <QItemSelection>

namespace {

class ProxyTreeDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void initStyleOption(QStyleOptionViewItem *option,
                         const QModelIndex &index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);

        ThemeManager &tm = ThemeManager::instance();
        const QString state = index.data(Qt::UserRole + 2).toString();

        if (index.column() == 0 && state == "active") {
            option->palette.setColor(QPalette::Text, tm.getColor("success"));
        }

        if (index.column() == 2) {
            if (state == "loading")
                option->palette.setColor(QPalette::Text, tm.getColor("text-tertiary"));
            else if (state == "ok")
                option->palette.setColor(QPalette::Text, tm.getColor("success"));
            else if (state == "warn")
                option->palette.setColor(QPalette::Text, tm.getColor("warning"));
            else if (state == "bad")
                option->palette.setColor(QPalette::Text, tm.getColor("error"));
        }
    }
};

} // namespace

ProxyView::ProxyView(QWidget *parent)
    : QWidget(parent)
    , m_proxyService(nullptr)
    , m_delayTestService(nullptr)
    , m_testSelectedBtn(nullptr)
{
    setupUI();
    updateStyle();
    
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &ProxyView::updateStyle);
}

void ProxyView::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);


    QHBoxLayout *headerLayout = new QHBoxLayout;
    QVBoxLayout *titleLayout = new QVBoxLayout;
    titleLayout->setSpacing(4);

    QLabel *titleLabel = new QLabel(tr("Proxy"));
    titleLabel->setObjectName("PageTitle");
    QLabel *subtitleLabel = new QLabel(tr("Select proxy nodes and run latency tests"));
    subtitleLabel->setObjectName("PageSubtitle");

    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(subtitleLabel);
    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();

    mainLayout->addLayout(headerLayout);
    

    QFrame *toolbarCard = new QFrame;
    toolbarCard->setObjectName("ToolbarCard");
    QVBoxLayout *toolbarCardLayout = new QVBoxLayout(toolbarCard);
    toolbarCardLayout->setContentsMargins(14, 12, 14, 12);
    toolbarCardLayout->setSpacing(12);

    QHBoxLayout *toolbarLayout = new QHBoxLayout;
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(12);
    
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(tr("Search nodes..."));
    m_searchEdit->setObjectName("SearchInput");
    m_searchEdit->setClearButtonEnabled(true);
    
    m_testSelectedBtn = new QPushButton(tr("Test Selected"));
    m_testSelectedBtn->setObjectName("TestSelectedBtn");
    m_testSelectedBtn->setCursor(Qt::PointingHandCursor);
    
    m_testAllBtn = new QPushButton(tr("Test All"));
    m_testAllBtn->setObjectName("TestAllBtn");
    m_testAllBtn->setCursor(Qt::PointingHandCursor);
    
    m_refreshBtn = new QPushButton(tr("Refresh"));
    m_refreshBtn->setObjectName("RefreshBtn");
    m_refreshBtn->setCursor(Qt::PointingHandCursor);
    
    toolbarLayout->addWidget(m_searchEdit, 1);
    toolbarLayout->addWidget(m_testSelectedBtn);
    toolbarLayout->addWidget(m_testAllBtn);
    toolbarLayout->addWidget(m_refreshBtn);
    

    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(4);
    m_progressBar->hide();
    m_progressBar->setObjectName("ProxyProgress");
    
    toolbarCardLayout->addLayout(toolbarLayout);
    toolbarCardLayout->addWidget(m_progressBar);
    mainLayout->addWidget(toolbarCard);
    

    QFrame *treeCard = new QFrame;
    treeCard->setObjectName("TreeCard");
    QVBoxLayout *treeLayout = new QVBoxLayout(treeCard);
    treeLayout->setContentsMargins(12, 12, 12, 12);
    treeLayout->setSpacing(0);

    m_treeWidget = new QTreeWidget;
    m_treeWidget->setObjectName("ProxyTree");
    m_treeWidget->setHeaderLabels({"", "", ""});
    m_treeWidget->setRootIsDecorated(false);
    m_treeWidget->setIndentation(0);
    m_treeWidget->setAlternatingRowColors(false);
    m_treeWidget->setHeaderHidden(true);
    m_treeWidget->header()->setDefaultAlignment(Qt::AlignCenter);
    m_treeWidget->setFrameShape(QFrame::NoFrame);
    m_treeWidget->header()->setStretchLastSection(false);
    m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_treeWidget->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_treeWidget->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_treeWidget->header()->resizeSection(1, 100);
    m_treeWidget->header()->resizeSection(2, 100);
    m_treeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeWidget->setItemDelegate(new ProxyTreeDelegate(m_treeWidget));
    connect(m_treeWidget->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ProxyView::onSelectionChanged);
    
    treeLayout->addWidget(m_treeWidget);
    mainLayout->addWidget(treeCard, 1);
    

    connect(m_searchEdit, &QLineEdit::textChanged, this, &ProxyView::onSearchTextChanged);
    connect(m_testSelectedBtn, &QPushButton::clicked, this, &ProxyView::onTestSelectedClicked);
    connect(m_testAllBtn, &QPushButton::clicked, this, &ProxyView::onTestAllClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &ProxyView::refresh);
    connect(m_treeWidget, &QTreeWidget::itemClicked, this, &ProxyView::onItemClicked);
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked, this, &ProxyView::onItemDoubleClicked);
}

void ProxyView::updateStyle()
{
    ThemeManager &tm = ThemeManager::instance();
    
    setStyleSheet(tm.loadStyleSheet(":/styles/proxy_view.qss"));

    applyTreeItemColors();
    if (m_treeWidget && m_treeWidget->viewport()) {
        m_treeWidget->viewport()->update();
    }
    updateTestButtonStyle(m_delayTestService && m_delayTestService->isTesting());
}

void ProxyView::setProxyService(ProxyService *service)
{
    m_proxyService = service;
    
    if (m_proxyService) {

        if (!m_delayTestService) {
            m_delayTestService = new DelayTestService(this);
            connect(m_delayTestService, &DelayTestService::nodeDelayResult,
                    this, &ProxyView::onDelayResult);
            connect(m_delayTestService, &DelayTestService::testProgress,
                    this, &ProxyView::onTestProgress);
            connect(m_delayTestService, &DelayTestService::testCompleted,
                    this, &ProxyView::onTestCompleted);
        }
        
        m_delayTestService->setApiPort(m_proxyService->getApiPort());
        m_delayTestService->setApiToken(m_proxyService->getApiToken());
        
        connect(m_proxyService, &ProxyService::proxiesReceived, 
                this, &ProxyView::onProxiesReceived);
        connect(m_proxyService, &ProxyService::proxySelected,
                this, &ProxyView::onProxySelected);
        connect(m_proxyService, &ProxyService::proxySelectFailed,
                this, &ProxyView::onProxySelectFailed);
    }
}

void ProxyView::refresh()
{
    if (m_proxyService) {
        m_proxyService->fetchProxies();
    }
}

void ProxyView::onProxiesReceived(const QJsonObject &proxies)
{
    renderProxies(proxies);
}

void ProxyView::renderProxies(const QJsonObject &proxies)
{

    QSet<QString> expandedGroups;
    QString selectedNode;
    
    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        if ((*it)->isExpanded()) {
            QString groupName = (*it)->data(0, Qt::UserRole + 1).toString();
            expandedGroups.insert(groupName.isEmpty() ? (*it)->text(0) : groupName);
        }
        if ((*it)->isSelected()) {
            selectedNode = nodeDisplayName(*it);
            if (selectedNode.startsWith("* ")) {
                selectedNode = selectedNode.mid(2);
            }
        }
        ++it;
    }
    
    m_treeWidget->clear();
    m_cachedProxies = proxies["proxies"].toObject();
    ThemeManager &tm = ThemeManager::instance();
    
    for (auto it = m_cachedProxies.begin(); it != m_cachedProxies.end(); ++it) {
        QJsonObject proxy = it.value().toObject();
        QString type = proxy["type"].toString();
        

        if (type == "Selector" || type == "URLTest" || type == "Fallback") {
            QTreeWidgetItem *groupItem = new QTreeWidgetItem(m_treeWidget);
            groupItem->setText(0, QString()); // Fix ghosting: clear text, use widget only
            groupItem->setText(1, type);
            groupItem->setData(0, Qt::UserRole, "group");
            groupItem->setData(0, Qt::UserRole + 1, it.key()); // Store key for logic
            groupItem->setFlags(groupItem->flags() & ~Qt::ItemIsSelectable);
            groupItem->setFirstColumnSpanned(true);
            
            // Group Item Style
            QFont font = groupItem->font(0);
            font.setBold(true);
            groupItem->setFont(0, font);
            

            if (expandedGroups.contains(it.key())) {
                groupItem->setExpanded(true);
            }
            

            QJsonArray all = proxy["all"].toArray();
            QString now = proxy["now"].toString();

            QWidget *treeViewport = m_treeWidget->viewport();
            QFrame *groupCard = new QFrame(treeViewport);
            groupCard->setObjectName("ProxyGroupCard");
            QHBoxLayout *cardLayout = new QHBoxLayout(groupCard);
            cardLayout->setContentsMargins(14, 12, 14, 12);
            cardLayout->setSpacing(10);

            QLabel *titleLabel = new QLabel(it.key(), groupCard);
            titleLabel->setObjectName("ProxyGroupTitle");

            QLabel *typeLabel = new QLabel(type, groupCard);
            typeLabel->setAlignment(Qt::AlignCenter);
            typeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

            QLabel *countLabel = new QLabel(tr("%1 nodes").arg(all.size()), groupCard);
            countLabel->setObjectName("ProxyGroupMeta");

            QLabel *currentLabel = new QLabel(now.isEmpty() ? QString() : tr("Current: %1").arg(now), groupCard);
            currentLabel->setObjectName("ProxyGroupCurrent");
            currentLabel->setVisible(!now.isEmpty());

            cardLayout->addWidget(titleLabel);
            cardLayout->addWidget(typeLabel);
            cardLayout->addSpacing(6);
            cardLayout->addWidget(countLabel);
            cardLayout->addSpacing(6);
            cardLayout->addWidget(currentLabel);
            cardLayout->addStretch();

            ChevronToggle *toggleBtn = new ChevronToggle(groupCard);
            toggleBtn->setObjectName("ProxyGroupToggle");
            toggleBtn->setExpanded(groupItem->isExpanded());
            toggleBtn->setFixedSize(28, 28);
            cardLayout->addWidget(toggleBtn);

            groupItem->setSizeHint(0, QSize(0, 72));
            groupItem->setText(1, QString());
            groupItem->setText(2, QString());
            m_treeWidget->setItemWidget(groupItem, 0, groupCard);

            connect(toggleBtn, &ChevronToggle::toggled, this, [groupItem](bool expanded) {
                groupItem->setExpanded(expanded);
            });
            
            for (const auto &nodeName : all) {
                QString name = nodeName.toString();
                QTreeWidgetItem *nodeItem = new QTreeWidgetItem(groupItem);

                nodeItem->setText(0, QString());
                nodeItem->setText(1, QString());
                nodeItem->setText(2, QString());
                nodeItem->setFirstColumnSpanned(true);
                nodeItem->setData(0, Qt::UserRole, "node");
                nodeItem->setData(0, Qt::UserRole + 1, it.key());
                nodeItem->setData(0, Qt::UserRole + 3, name); // store display name

                if (name == selectedNode) {
                    nodeItem->setSelected(true);
                }

                QString nodeType;
                QString delayText;
                if (m_cachedProxies.contains(name)) {
                    QJsonObject nodeProxy = m_cachedProxies[name].toObject();
                    nodeType = nodeProxy["type"].toString();

                    if (nodeProxy.contains("history")) {
                        QJsonArray history = nodeProxy["history"].toArray();
                        if (!history.isEmpty()) {
                            int delay = history.last().toObject()["delay"].toInt();
                            delayText = formatDelay(delay);
                        }
                    }
                }

                nodeItem->setText(2, delayText);
                QWidget *rowCard = buildNodeRow(name, nodeType, delayText);
                nodeItem->setSizeHint(0, rowCard->sizeHint());
                m_treeWidget->setItemWidget(nodeItem, 0, rowCard);
                updateNodeRowSelected(nodeItem, nodeItem->isSelected());
            }
        }
    }
    applyTreeItemColors();
}

void ProxyView::onItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    handleNodeActivation(item);
}

void ProxyView::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    handleNodeActivation(item);
}

void ProxyView::applyTreeItemColors()
{
    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        QTreeWidgetItem *item = *it;
        const QString role = item->data(0, Qt::UserRole).toString();
        if (role == "node") {
            QString name = item->data(0, Qt::UserRole + 3).toString();
            if (name.isEmpty()) {
                name = item->text(0);
            }
            bool hasStar = name.startsWith("* ");
            if (hasStar) name = name.mid(2);

            const QString group = item->data(0, Qt::UserRole + 1).toString();
            const QString now = m_cachedProxies.value(group).toObject().value("now").toString();
            markNodeState(item, group, now, item->text(2));
        }
        ++it;
    }
}

void ProxyView::markNodeState(QTreeWidgetItem *item, const QString &group, const QString &now, const QString &delayText)
{
    Q_UNUSED(group);
    if (!item) return;

    QString baseName = item->data(0, Qt::UserRole + 3).toString();
    if (baseName.isEmpty()) {
        baseName = item->text(0);
    }
    bool hasStar = baseName.startsWith("* ");
    if (hasStar) baseName = baseName.mid(2);

    const bool isActive = (baseName == now);
    item->setData(0, Qt::UserRole + 2, isActive ? "active" : "");
    QString displayName = isActive ? "* " + baseName : baseName;

    // delay state marker
    QString state = "loading";
    if (!delayText.isEmpty() && delayText != "...") {
        QString delayStr = delayText;
        delayStr.remove(" ms");
        bool ok = false;
        const int delay = delayStr.toInt(&ok);
        if (ok) {
            if (delay <= 0) state = "bad";
            else if (delay < 100) state = "ok";
            else if (delay < 300) state = "warn";
            else state = "bad";
        } else {
            state.clear();
        }
    }
    item->setData(2, Qt::UserRole + 2, state);
    if (QWidget *row = m_treeWidget->itemWidget(item, 0)) {
        if (auto *nameLabel = row->findChild<QLabel*>("ProxyNodeName")) {
            nameLabel->setText(displayName);
        }
    }
    updateNodeRowDelay(item, delayText, state);
    updateNodeRowSelected(item, item->isSelected());
}

void ProxyView::handleNodeActivation(QTreeWidgetItem *item)
{
    if (!item || !m_proxyService) return;

    const QString role = item->data(0, Qt::UserRole).toString();
    if (role != "node") {
        return;
    }

    QString group = item->data(0, Qt::UserRole + 1).toString();
    QString nodeName = nodeDisplayName(item);
    if (nodeName.startsWith("* ")) nodeName = nodeName.mid(2);

    m_pendingSelection.insert(group, nodeName);
    m_proxyService->selectProxy(group, nodeName);
}

void ProxyView::onProxySelected(const QString &group, const QString &proxy)
{
    if (!m_pendingSelection.contains(group) || m_pendingSelection.value(group) != proxy) {
        return;
    }

    m_pendingSelection.remove(group);
    updateSelectedProxyUI(group, proxy);
}

void ProxyView::onProxySelectFailed(const QString &group, const QString &proxy)
{
    if (m_pendingSelection.value(group) == proxy) {
        m_pendingSelection.remove(group);
    }
}

void ProxyView::onTestSelectedClicked()
{
    if (!m_delayTestService || !m_treeWidget) return;

    // 避免与批量测试并行冲突。
    if (m_delayTestService->isTesting() && !m_testingNodes.isEmpty()) {
        return;
    }

    const auto selected = m_treeWidget->selectedItems();
    if (selected.isEmpty()) return;

    QTreeWidgetItem *item = selected.first();
    if (!item || item->data(0, Qt::UserRole).toString() != "node") {
        return;
    }

    QString name = nodeDisplayName(item);
    if (name.startsWith("* ")) {
        name = name.mid(2);
    }

    if (name == "DIRECT" || name == "REJECT" || name == "COMPATIBLE") {
        return;
    }

    item->setText(2, "...");
    item->setData(2, Qt::UserRole + 2, QString("loading"));

    m_testingNodes.insert(name);
    m_singleTesting = true;
    m_singleTestingTarget = name;
    if (m_testSelectedBtn) {
        m_testSelectedBtn->setEnabled(false);
        m_testSelectedBtn->setProperty("testing", true);
        m_testSelectedBtn->style()->unpolish(m_testSelectedBtn);
        m_testSelectedBtn->style()->polish(m_testSelectedBtn);
    }

    DelayTestOptions options;
    options.timeoutMs = AppSettings::instance().urltestTimeoutMs();
    options.url = AppSettings::instance().urltestUrl();
    options.samples = AppSettings::instance().urltestSamples();
    options.concurrency = 1;
    m_delayTestService->testNodeDelay(name, options);
}

void ProxyView::onTestAllClicked()
{
    if (!m_delayTestService) return;
    if (m_singleTesting) return;
    
    if (m_delayTestService->isTesting()) {
        m_delayTestService->stopAllTests();
        m_testAllBtn->setText(tr("Test All"));
        updateTestButtonStyle(false);
        return;
    }
    

    QStringList nodesToTest;
    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        if ((*it)->data(0, Qt::UserRole).toString() == "node") {
            QString name = nodeDisplayName(*it);
            if (name.startsWith("* ")) {
                name = name.mid(2);
            }

            if (name != "DIRECT" && name != "REJECT" && name != "COMPATIBLE") {
                nodesToTest.append(name);
            }
            
            (*it)->setText(2, "...");
            (*it)->setData(2, Qt::UserRole + 2, QString("loading"));
        }
        ++it;
    }
    
    if (nodesToTest.isEmpty()) return;
    

    nodesToTest.removeDuplicates();
    
    m_testingNodes.clear();
    for(const QString& node : nodesToTest) {
        m_testingNodes.insert(node);
    } 
    

    m_testAllBtn->setText(tr("Stop Tests"));
    updateTestButtonStyle(true);
    m_progressBar->show();
    m_progressBar->setValue(0);
    

    DelayTestOptions options;
    options.timeoutMs = AppSettings::instance().urltestTimeoutMs();
    options.url = AppSettings::instance().urltestUrl();
    options.samples = AppSettings::instance().urltestSamples();
    options.concurrency = AppSettings::instance().urltestConcurrency();
    
    m_delayTestService->testNodesDelay(nodesToTest, options);
}

void ProxyView::onSearchTextChanged(const QString &text)
{
    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        if ((*it)->data(0, Qt::UserRole).toString() == "node") {
             bool match = nodeDisplayName(*it).contains(text, Qt::CaseInsensitive);
             (*it)->setHidden(!match && !text.isEmpty());
        }
        ++it;
    }

    if (!text.isEmpty()) {
        it = QTreeWidgetItemIterator(m_treeWidget);
        while (*it) {
            if ((*it)->data(0, Qt::UserRole).toString() == "group") {
                bool hasVisibleChild = false;
                for(int i=0; i<(*it)->childCount(); ++i) {
                    if (!(*it)->child(i)->isHidden()) {
                        hasVisibleChild = true;
                        break;
                    }
                }
                (*it)->setExpanded(hasVisibleChild);
                (*it)->setHidden(!hasVisibleChild);
            }
            ++it;
        }
    }
}

void ProxyView::onDelayResult(const ProxyDelayTestResult &result)
{
    QString displayText;
    
    
    if (result.ok) {
        displayText = formatDelay(result.delay);
    } else {
        displayText = tr("Timeout");
    }
    

    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        QString name = nodeDisplayName(*it);
        if (name.startsWith("* ")) {
            name = name.mid(2);
        }
        
        if (name == result.proxy) {
            const QString group = (*it)->data(0, Qt::UserRole + 1).toString();
            const QString now = m_cachedProxies.value(group).toObject().value("now").toString();
            markNodeState(*it, group, now, displayText);
        }
        ++it;
    }
    
    m_testingNodes.remove(result.proxy);

    if (m_singleTesting && result.proxy == m_singleTestingTarget) {
        m_singleTesting = false;
        m_singleTestingTarget.clear();
        if (m_testSelectedBtn) {
            m_testSelectedBtn->setEnabled(true);
            m_testSelectedBtn->setProperty("testing", false);
            m_testSelectedBtn->style()->unpolish(m_testSelectedBtn);
            m_testSelectedBtn->style()->polish(m_testSelectedBtn);
        }
    }

    // 更新选中态的视觉效果（防止延迟更新后样式丢失）
    if (QTreeWidgetItem *current = m_treeWidget->currentItem()) {
        updateNodeRowSelected(current, current->isSelected());
    }
}

void ProxyView::onTestProgress(int current, int total)
{
    if (total > 0) {
        int progress = (current * 100) / total;
        m_progressBar->setValue(progress);
    }
}

void ProxyView::onTestCompleted()
{
    m_testAllBtn->setText(tr("Test All"));
    updateTestButtonStyle(false);
    m_progressBar->hide();
    m_testingNodes.clear();

    if (QTreeWidgetItem *current = m_treeWidget->currentItem()) {
        updateNodeRowSelected(current, current->isSelected());
    }
}

void ProxyView::updateSelectedProxyUI(const QString &group, const QString &proxy)
{
    if (group.isEmpty() || proxy.isEmpty() || !m_treeWidget) {
        return;
    }

    if (m_cachedProxies.contains(group)) {
        QJsonObject groupProxy = m_cachedProxies.value(group).toObject();
        groupProxy["now"] = proxy;
        m_cachedProxies[group] = groupProxy;
    }

    QTreeWidgetItem *groupItem = nullptr;
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_treeWidget->topLevelItem(i);
        if (item && item->data(0, Qt::UserRole + 1).toString() == group) {
            groupItem = item;
            break;
        }
    }

    if (!groupItem) {
        return;
    }

    QWidget *groupCard = m_treeWidget->itemWidget(groupItem, 0);
    if (groupCard) {
        QLabel *currentLabel = groupCard->findChild<QLabel *>("ProxyGroupCurrent");
        if (currentLabel) {
            currentLabel->setText(tr("Current: %1").arg(proxy));
            currentLabel->setVisible(true);
        }
    }

    for (int i = 0; i < groupItem->childCount(); ++i) {
        QTreeWidgetItem *child = groupItem->child(i);
        if (!child) continue;

        QString name = child->text(0);
        if (name.startsWith("* ")) {
            name = name.mid(2);
        }

        if (name == proxy) {
            child->setText(0, "* " + name);
            child->setData(0, Qt::UserRole + 2, "active");
        } else {
            child->setText(0, name);
            child->setData(0, Qt::UserRole + 2, "");
        }
    }
}

QString ProxyView::formatDelay(int delay) const
{
    if (delay <= 0) return tr("Timeout");
    return QString::number(delay) + " ms";
}

QString ProxyView::nodeDisplayName(QTreeWidgetItem *item) const
{
    if (!item) return QString();
    QString name = item->data(0, Qt::UserRole + 3).toString();
    if (name.isEmpty()) {
        name = item->text(0);
    }
    return name;
}

QWidget* ProxyView::buildNodeRow(const QString &name, const QString &type, const QString &delay) const
{
    QWidget *card = new QFrame(m_treeWidget->viewport());
    card->setObjectName("ProxyNodeCard");
    card->setAttribute(Qt::WA_Hover, true);
    card->setMouseTracking(true);
    card->setProperty("selected", false);
    QHBoxLayout *layout = new QHBoxLayout(card);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(10);

    QLabel *nameLabel = new QLabel(name, card);
    nameLabel->setObjectName("ProxyNodeName");

    QLabel *typeLabel = new QLabel(type, card);
    typeLabel->setObjectName("ProxyNodeType");
    typeLabel->setAlignment(Qt::AlignCenter);
    typeLabel->setMinimumWidth(64);

    QLabel *delayLabel = new QLabel(delay, card);
    delayLabel->setObjectName("ProxyNodeDelay");
    delayLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    layout->addWidget(nameLabel, 1);
    layout->addWidget(typeLabel);
    layout->addStretch();
    layout->addWidget(delayLabel, 0, Qt::AlignRight);

    return card;
}

void ProxyView::updateNodeRowDelay(QTreeWidgetItem *item, const QString &delayText, const QString &state)
{
    if (!item) return;
    QWidget *row = m_treeWidget->itemWidget(item, 0);
    if (!row) return;
    if (auto *delayLabel = row->findChild<QLabel *>("ProxyNodeDelay")) {
        delayLabel->setText(delayText);
        delayLabel->setProperty("state", state);
        delayLabel->style()->unpolish(delayLabel);
        delayLabel->style()->polish(delayLabel);
    }
}

void ProxyView::updateNodeRowSelected(QTreeWidgetItem *item, bool selected)
{
    if (!item) return;
    QWidget *row = m_treeWidget->itemWidget(item, 0);
    if (!row) return;
    row->setProperty("selected", selected);
    row->style()->unpolish(row);
    row->style()->polish(row);
}

void ProxyView::testSingleNode(const QString &proxy)
{
    if (!m_delayTestService) return;
    m_delayTestService->testNodeDelay(proxy);
}

void ProxyView::updateTestButtonStyle(bool testing)
{
    if (!m_testAllBtn) return;

    m_testAllBtn->setProperty("testing", testing);
    m_testAllBtn->style()->unpolish(m_testAllBtn);
    m_testAllBtn->style()->polish(m_testAllBtn);

    if (m_testSelectedBtn) {
        const bool busy = testing || m_singleTesting;
        m_testSelectedBtn->setEnabled(!busy);
        m_testSelectedBtn->setProperty("testing", busy);
        m_testSelectedBtn->style()->unpolish(m_testSelectedBtn);
        m_testSelectedBtn->style()->polish(m_testSelectedBtn);
    }
}

void ProxyView::onSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    for (const QModelIndex &idx : selected.indexes()) {
        if (idx.column() != 0) continue;
        if (auto *item = m_treeWidget->itemFromIndex(idx)) {
            if (item->data(0, Qt::UserRole).toString() == "node") {
                updateNodeRowSelected(item, true);
            }
        }
    }
    for (const QModelIndex &idx : deselected.indexes()) {
        if (idx.column() != 0) continue;
        if (auto *item = m_treeWidget->itemFromIndex(idx)) {
            if (item->data(0, Qt::UserRole).toString() == "node") {
                updateNodeRowSelected(item, false);
            }
        }
    }
}
