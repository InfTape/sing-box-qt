#include "ProxyView.h"
#include "core/ProxyService.h"
#include "core/DelayTestService.h"
#include "utils/ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTimer>
#include <QMessageBox>
#include <QLabel>
#include <QToolButton>

ProxyView::ProxyView(QWidget *parent)
    : QWidget(parent)
    , m_proxyService(nullptr)
    , m_delayTestService(nullptr)
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


    QLabel *titleLabel = new QLabel(tr("Proxy"));
    titleLabel->setObjectName("PageTitle");
    QLabel *subtitleLabel = new QLabel(tr("Select proxy nodes and run latency tests"));
    subtitleLabel->setObjectName("PageSubtitle");
    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(subtitleLabel);
    

    QFrame *toolbarCard = new QFrame;
    toolbarCard->setObjectName("ToolbarCard");
    QVBoxLayout *toolbarCardLayout = new QVBoxLayout(toolbarCard);
    toolbarCardLayout->setContentsMargins(16, 16, 16, 12);
    toolbarCardLayout->setSpacing(12);

    QHBoxLayout *toolbarLayout = new QHBoxLayout;
    
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(tr("Search nodes..."));
    m_searchEdit->setObjectName("SearchInput");
    
    m_testAllBtn = new QPushButton(tr("Test All"));
    m_testAllBtn->setCursor(Qt::PointingHandCursor);
    m_testAllBtn->setObjectName("ActionBtn");
    
    m_refreshBtn = new QPushButton(tr("Refresh"));
    m_refreshBtn->setCursor(Qt::PointingHandCursor);
    m_refreshBtn->setObjectName("ActionBtn");
    
    toolbarLayout->addWidget(m_searchEdit, 1);
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
    m_treeWidget->setStyleSheet("QTreeView::item { height: 36px; }");
    
    treeLayout->addWidget(m_treeWidget);
    mainLayout->addWidget(treeCard, 1);
    

    connect(m_searchEdit, &QLineEdit::textChanged, this, &ProxyView::onSearchTextChanged);
    connect(m_testAllBtn, &QPushButton::clicked, this, &ProxyView::onTestAllClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &ProxyView::refresh);
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked, this, &ProxyView::onItemDoubleClicked);
}

void ProxyView::updateStyle()
{
    ThemeManager &tm = ThemeManager::instance();
    
    m_searchEdit->setStyleSheet(tm.getInputStyle());
    m_testAllBtn->setStyleSheet(tm.getButtonStyle());
    m_refreshBtn->setStyleSheet(tm.getButtonStyle());
    
    setStyleSheet(QString(R"(
        #PageTitle {
            font-size: 22px;
            font-weight: bold;
            color: %1;
        }
        #PageSubtitle {
            font-size: 13px;
            color: %2;
        }
        #ToolbarCard, #TreeCard {
            background-color: %3;
            border: 1px solid #353b43;
            border-radius: 12px;
        }
        #ProxyGroupCard {
            background-color: %3;
            border: 1px solid #353b43;
            border-radius: 12px;
        }
        #ProxyGroupTitle {
            font-size: 15px;
            font-weight: 600;
            color: %1;
        }
        #ProxyGroupMeta {
            font-size: 12px;
            color: %2;
        }
        #ProxyGroupBadge {
            padding: 4px 8px;
            border-radius: 10px;
            background-color: %4;
            color: %1;
            font-size: 11px;
            font-weight: 600;
        }
        #ProxyGroupToggle {
            width: 28px;
            height: 28px;
            border-radius: 6px;
            background-color: %4;
            color: %1;
            border: none;
        }
        #ProxyGroupToggle:hover {
            background-color: %4;
            opacity: 0.8;
        }
        #ProxyProgress {
            background-color: %4;
            border-radius: 6px;
        }
        #ProxyProgress::chunk {
            background-color: %5;
            border-radius: 6px;
        }
    )")
    .arg(tm.getColorString("text-primary"))
    .arg(tm.getColorString("text-secondary"))
    .arg(tm.getColorString("bg-secondary"))
    .arg(tm.getColorString("bg-tertiary"))
    .arg(tm.getColorString("primary"))
    );

    m_treeWidget->setStyleSheet(QString(R"(
        QTreeWidget {
            background-color: transparent;
            border: none;
            color: %1;
            outline: none;
        }
        QTreeView::branch {
            image: none;
            background: transparent;
        }
        QTreeView::branch:open, QTreeView::branch:closed {
            image: none;
        }
        QTreeWidget::item {
            padding: 8px;
            border-radius: 10px;
            margin: 2px 4px;
            color: #ffffff;
        }
        QTreeWidget::item:selected {
            background-color: %2;
            color: %3;
        }
        QTreeWidget::item:hover:!selected {
            background-color: %2;
            color: %3;
        }
        QTreeWidget::header, QHeaderView {
            background: transparent;
        }
        QHeaderView::section {
            background-color: transparent;
            color: %4;
            padding: 8px;
            border: none;
            font-weight: bold;
        }
        QHeaderView::section:first {
            border-top-left-radius: 10px;
        }
        QHeaderView::section:last {
            border-top-right-radius: 10px;
        }
    )")
    .arg(tm.getColorString("text-primary"))
    .arg(tm.getColorString("primary") + "30")
    .arg(tm.getColorString("primary"))
    .arg(tm.getColorString("text-secondary"))
    );
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
        
        connect(m_proxyService, &ProxyService::proxiesReceived, 
                this, &ProxyView::onProxiesReceived);
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

    QSet<QString> expandedGroups;
    QString selectedNode;
    
    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        if ((*it)->isExpanded()) {
            QString groupName = (*it)->data(0, Qt::UserRole + 1).toString();
            expandedGroups.insert(groupName.isEmpty() ? (*it)->text(0) : groupName);
        }
        if ((*it)->isSelected()) {
            selectedNode = (*it)->text(0);
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

            QFrame *groupCard = new QFrame;
            groupCard->setObjectName("ProxyGroupCard");
            QHBoxLayout *cardLayout = new QHBoxLayout(groupCard);
            cardLayout->setContentsMargins(14, 12, 14, 12);
            cardLayout->setSpacing(10);

            QLabel *titleLabel = new QLabel(it.key());
            titleLabel->setObjectName("ProxyGroupTitle");

            QLabel *typeLabel = new QLabel(type);
            typeLabel->setObjectName("ProxyGroupBadge");

            QLabel *countLabel = new QLabel(tr("%1 nodes").arg(all.size()));
            countLabel->setObjectName("ProxyGroupMeta");

            QLabel *currentLabel = new QLabel(now.isEmpty() ? QString() : tr("Current: %1").arg(now));
            currentLabel->setObjectName("ProxyGroupMeta");

            cardLayout->addWidget(titleLabel);
            cardLayout->addWidget(typeLabel);
            cardLayout->addSpacing(6);
            cardLayout->addWidget(countLabel);
            if (!now.isEmpty()) {
                cardLayout->addSpacing(6);
                cardLayout->addWidget(currentLabel);
            }
            cardLayout->addStretch();

            QToolButton *toggleBtn = new QToolButton;
            toggleBtn->setObjectName("ProxyGroupToggle");
            toggleBtn->setArrowType(groupItem->isExpanded() ? Qt::DownArrow : Qt::RightArrow);
            toggleBtn->setCursor(Qt::PointingHandCursor);
            toggleBtn->setAutoRaise(true);
            cardLayout->addWidget(toggleBtn);

            groupItem->setSizeHint(0, QSize(0, 72));
            groupItem->setText(1, QString());
            groupItem->setText(2, QString());
            m_treeWidget->setItemWidget(groupItem, 0, groupCard);

            connect(toggleBtn, &QToolButton::clicked, this, [groupItem, toggleBtn]() {
                groupItem->setExpanded(!groupItem->isExpanded());
                toggleBtn->setArrowType(groupItem->isExpanded() ? Qt::DownArrow : Qt::RightArrow);
            });
            
            for (const auto &nodeName : all) {
                QString name = nodeName.toString();
                QTreeWidgetItem *nodeItem = new QTreeWidgetItem(groupItem);
                

                if (name == now) {
                    nodeItem->setText(0, "* " + name);
                    nodeItem->setForeground(0, tm.getColor("success"));
                } else {
                    nodeItem->setText(0, name);
                }
                
                nodeItem->setData(0, Qt::UserRole, "node");
                nodeItem->setData(0, Qt::UserRole + 1, it.key());
                

                if (name == selectedNode) {
                    nodeItem->setSelected(true);
                }
                

                if (m_cachedProxies.contains(name)) {
                    QJsonObject nodeProxy = m_cachedProxies[name].toObject();
                    nodeItem->setText(1, nodeProxy["type"].toString());
                    nodeItem->setForeground(1, tm.getColor("text-tertiary"));
                    

                    if (nodeProxy.contains("history")) {
                        QJsonArray history = nodeProxy["history"].toArray();
                        if (!history.isEmpty()) {
                            int delay = history.last().toObject()["delay"].toInt();
                            nodeItem->setText(2, formatDelay(delay));
                            nodeItem->setForeground(2, getDelayColor(delay));
                        }
                    }
                }
            }
        }
    }
}

void ProxyView::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    
    if (!m_proxyService) return;
    
    QString role = item->data(0, Qt::UserRole).toString();
    
    if (role == "node") {
        QString group = item->data(0, Qt::UserRole + 1).toString();
        QString nodeName = item->text(0);
        

        if (nodeName.startsWith("* ")) {
            nodeName = nodeName.mid(2);
        }
        
        m_proxyService->selectProxy(group, nodeName);
        

        QTimer::singleShot(200, this, &ProxyView::refresh);
    }
}

void ProxyView::onTestAllClicked()
{
    if (!m_delayTestService) return;
    
    if (m_delayTestService->isTesting()) {
        m_delayTestService->stopAllTests();
        m_testAllBtn->setText(tr("Test All"));
        return;
    }
    

    QStringList nodesToTest;
    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        if ((*it)->data(0, Qt::UserRole).toString() == "node") {
            QString name = (*it)->text(0);
            if (name.startsWith("* ")) {
                name = name.mid(2);
            }

            if (name != "DIRECT" && name != "REJECT" && name != "COMPATIBLE") {
                nodesToTest.append(name);
            }
            

            (*it)->setText(2, "...");
            (*it)->setForeground(2, QColor("#888888"));
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
    m_progressBar->show();
    m_progressBar->setValue(0);
    

    DelayTestOptions options;
    options.timeoutMs = 5000;
    options.concurrency = 6;
    
    m_delayTestService->testNodesDelay(nodesToTest, options);
}

void ProxyView::onSearchTextChanged(const QString &text)
{
    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        if ((*it)->data(0, Qt::UserRole).toString() == "node") {
             bool match = (*it)->text(0).contains(text, Qt::CaseInsensitive);
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
    QColor color;
    
    if (result.ok) {
        displayText = formatDelay(result.delay);
        color = getDelayColor(result.delay);
    } else {
        displayText = tr("Timeout");
        color = ThemeManager::instance().getColor("error");
    }
    

    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        QString name = (*it)->text(0);
        if (name.startsWith("* ")) {
            name = name.mid(2);
        }
        
        if (name == result.proxy) {
            (*it)->setText(2, displayText);
            (*it)->setForeground(2, color);
        }
        ++it;
    }
    
    m_testingNodes.remove(result.proxy);
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
    m_progressBar->hide();
    m_testingNodes.clear();
}

QString ProxyView::formatDelay(int delay) const
{
    if (delay <= 0) return tr("Timeout");
    return QString::number(delay) + " ms";
}

QColor ProxyView::getDelayColor(int delay) const
{
    ThemeManager &tm = ThemeManager::instance();
    if (delay <= 0) return tm.getColor("error");
    if (delay < 100) return tm.getColor("success");
    if (delay < 300) return tm.getColor("warning");
    return tm.getColor("error");
}

void ProxyView::testSingleNode(const QString &proxy)
{
    if (!m_delayTestService) return;
    m_delayTestService->testNodeDelay(proxy);
}
