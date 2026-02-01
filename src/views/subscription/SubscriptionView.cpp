#include "SubscriptionView.h"
#include "dialogs/config/ConfigEditDialog.h"
#include "dialogs/subscription/SubscriptionFormDialog.h"
#include "network/SubscriptionService.h"
#include "utils/ThemeManager.h"
#include "views/subscription/SubscriptionCard.h"
#include "dialogs/subscription/NodeEditDialog.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QMenu>
#include <QJsonArray>
#include <QJsonDocument>
#include <QHash>
#include "utils/subscription/SubscriptionActions.h"
#include "utils/subscription/SubscriptionHelpers.h"
#include "utils/layout/CardGridLayoutHelper.h"
#include "utils/layout/CardGridAnimation.h"

// ==================== SubscriptionView ====================

SubscriptionView::SubscriptionView(QWidget *parent)
    : QWidget(parent)
    , m_subscriptionService(new SubscriptionService(this))
    , m_autoUpdateTimer(new QTimer(this))
{
    setupUI();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &SubscriptionView::updateStyle);
    updateStyle();

    m_autoUpdateTimer->setInterval(30 * 60 * 1000);
    connect(m_autoUpdateTimer, &QTimer::timeout, this, &SubscriptionView::onAutoUpdateTimer);
    m_autoUpdateTimer->start();
}

SubscriptionService* SubscriptionView::getService() const
{
    return m_subscriptionService;
}

void SubscriptionView::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    QHBoxLayout *headerLayout = new QHBoxLayout;
    QVBoxLayout *titleLayout = new QVBoxLayout;

    QLabel *titleLabel = new QLabel(tr("Subscription Manager"));
    titleLabel->setObjectName("PageTitle");

    QLabel *subtitleLabel = new QLabel(tr("Manage your subscriptions and proxy nodes"));
    subtitleLabel->setObjectName("PageSubtitle");

    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(subtitleLabel);
    titleLayout->setSpacing(6);

    m_addBtn = new QPushButton(tr("+ Add Subscription"));
    m_addBtn->setCursor(Qt::PointingHandCursor);
    m_addBtn->setMinimumSize(110, 36);
    m_addBtn->setObjectName("AddSubscriptionBtn");

    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    headerLayout->addWidget(m_addBtn);

    mainLayout->addLayout(headerLayout);



    m_scrollArea = new QScrollArea;
    m_scrollArea->setObjectName("SubscriptionScroll");
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_cardsContainer = new QWidget;
    m_cardsContainer->setObjectName("SubscriptionCards");
    m_cardsLayout = new QGridLayout(m_cardsContainer);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(16);
    m_cardsLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_scrollArea->setWidget(m_cardsContainer);

    mainLayout->addWidget(m_scrollArea, 1);

    connect(m_addBtn, &QPushButton::clicked, this, &SubscriptionView::onAddClicked);


    connect(m_subscriptionService, &SubscriptionService::subscriptionAdded,
            this, &SubscriptionView::refreshList);
    connect(m_subscriptionService, &SubscriptionService::subscriptionRemoved,
            this, &SubscriptionView::refreshList);
    connect(m_subscriptionService, &SubscriptionService::subscriptionUpdated,
            this, &SubscriptionView::refreshList);
    connect(m_subscriptionService, &SubscriptionService::activeSubscriptionChanged,
            this, &SubscriptionView::refreshList);
    connect(m_subscriptionService, &SubscriptionService::errorOccurred,
            this, [this](const QString &err) {
                QMessageBox::warning(this, tr("Notice"), err);
            });

    refreshList();
}

void SubscriptionView::updateStyle()
{
    ThemeManager &tm = ThemeManager::instance();
    setStyleSheet(tm.loadStyleSheet(":/styles/subscription_view.qss"));
}

void SubscriptionView::onAddClicked()
{
    QMenu menu(this);
    menu.addAction(tr("Add Subscription URL"), this, &SubscriptionView::openSubscriptionDialog);
    menu.addAction(tr("Add Manual Node"), this, &SubscriptionView::onAddNodeClicked);
    menu.exec(m_addBtn->mapToGlobal(QPoint(0, m_addBtn->height())));
}

void SubscriptionView::openSubscriptionDialog()
{
    SubscriptionFormDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString error;
    if (!dialog.validateInput(&error)) {
        QMessageBox::warning(this, tr("Notice"), error);
        return;
    }

    const bool isManual = dialog.isManual();
    const bool useOriginal = dialog.useOriginalConfig();
    const int interval = dialog.autoUpdateIntervalMinutes();

    if (isManual) {
        const QString content = dialog.isUriList() ? dialog.uriContent() : dialog.manualContent();
        m_subscriptionService->addManualSubscription(content,
                                                     dialog.name(),
                                                     useOriginal,
                                                     dialog.isUriList(),
                                                     true,
                                                     dialog.sharedRulesEnabled(),
                                                     dialog.ruleSets());
    } else {
        m_subscriptionService->addUrlSubscription(dialog.url(),
                                                 dialog.name(),
                                                 useOriginal,
                                                 interval,
                                                 true,
                                                 dialog.sharedRulesEnabled(),
                                                 dialog.ruleSets());
    }
}

void SubscriptionView::onAddNodeClicked()
{
    NodeEditDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QJsonObject node = dialog.nodeData();
        QJsonArray arr;
        arr.append(node);
        QJsonDocument doc(arr);
        QString content = doc.toJson(QJsonDocument::Compact);
        
        QString name = node["tag"].toString();
        m_subscriptionService->addManualSubscription(content,
                                                     name,
                                                     false,
                                                     false,
                                                     true,
                                                     dialog.sharedRulesEnabled(),
                                                     dialog.ruleSets());
    }
}

void SubscriptionView::onAutoUpdateTimer()
{
    const QList<SubscriptionInfo> subs = m_subscriptionService->getSubscriptions();
    const int activeIndex = m_subscriptionService->getActiveIndex();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (int i = 0; i < subs.count(); ++i) {
        const SubscriptionInfo &item = subs[i];
        if (item.isManual) continue;
        const int interval = item.autoUpdateIntervalMinutes;
        if (interval <= 0) continue;
        if (item.lastUpdate <= 0) continue;
        if (now - item.lastUpdate >= static_cast<qint64>(interval) * 60 * 1000) {
            m_subscriptionService->refreshSubscription(item.id, i == activeIndex);
        }
    }
}

SubscriptionCard* SubscriptionView::createSubscriptionCard(const SubscriptionInfo &info, bool active)
{
    SubscriptionCard *card = new SubscriptionCard(info, active, this);
    wireCardSignals(card);
    return card;
}

void SubscriptionView::wireCardSignals(SubscriptionCard *card)
{
    connect(card, &SubscriptionCard::useClicked, this, &SubscriptionView::handleUseSubscription);
    connect(card, &SubscriptionCard::editClicked, this, &SubscriptionView::handleEditSubscription);
    connect(card, &SubscriptionCard::editConfigClicked, this, &SubscriptionView::handleEditConfig);
    connect(card, &SubscriptionCard::refreshClicked, this, &SubscriptionView::handleRefreshSubscription);
    connect(card, &SubscriptionCard::rollbackClicked, this, &SubscriptionView::handleRollbackSubscription);
    connect(card, &SubscriptionCard::deleteClicked, this, &SubscriptionView::handleDeleteSubscription);
    connect(card, &SubscriptionCard::copyLinkClicked, this, &SubscriptionView::handleCopyLink);
}

void SubscriptionView::handleUseSubscription(const QString &id)
{
    SubscriptionActions::useSubscription(m_subscriptionService, id);
}

void SubscriptionView::handleEditSubscription(const QString &id)
{
    SubscriptionInfo target;
    if (!getSubscriptionById(id, &target)) return;

    QJsonObject singleNodeObj;
    const bool isSingleNode = SubscriptionHelpers::isSingleManualNode(target, &singleNodeObj);

    if (isSingleNode) {
        NodeEditDialog dialog(this);
        dialog.setRuleSets(target.ruleSets, target.enableSharedRules);
        dialog.setNodeData(singleNodeObj);
        if (dialog.exec() != QDialog::Accepted) return;

        QJsonObject newNode = dialog.nodeData();
        QJsonArray arr;
        arr.append(newNode);
        QString content = QJsonDocument(arr).toJson(QJsonDocument::Compact);
        QString name = newNode["tag"].toString();

        m_subscriptionService->updateSubscriptionMeta(
            id,
            name,
            target.url,
            true, // isManual
            content,
            target.useOriginalConfig,
            target.autoUpdateIntervalMinutes,
            dialog.sharedRulesEnabled(),
            dialog.ruleSets()
        );
    } else {
        SubscriptionFormDialog dialog(this);
        dialog.setEditData(target);
        if (dialog.exec() != QDialog::Accepted) return;

        QString error;
        if (!dialog.validateInput(&error)) {
            QMessageBox::warning(this, tr("Notice"), error);
            return;
        }

        const bool isManual = dialog.isManual();
        const QString content = dialog.isUriList() ? dialog.uriContent() : dialog.manualContent();
        m_subscriptionService->updateSubscriptionMeta(
            id,
            dialog.name(),
            dialog.url(),
            isManual,
            content,
            dialog.useOriginalConfig(),
            dialog.autoUpdateIntervalMinutes(),
            dialog.sharedRulesEnabled(),
            dialog.ruleSets()
        );
    }
}

void SubscriptionView::handleEditConfig(const QString &id)
{
    Q_UNUSED(id)
    const QString current = m_subscriptionService->getCurrentConfig();
    if (current.isEmpty()) {
        QMessageBox::warning(this, tr("Notice"), tr("Current config not found"));
        return;
    }
    ConfigEditDialog dialog(this);
    dialog.setContent(current);
    if (dialog.exec() == QDialog::Accepted) {
        if (!m_subscriptionService->saveCurrentConfig(dialog.content(), true)) {
            QMessageBox::warning(this, tr("Notice"), tr("Failed to save config"));
        }
    }
}

void SubscriptionView::handleRefreshSubscription(const QString &id, bool applyRuntime)
{
    SubscriptionActions::refreshSubscription(m_subscriptionService, id, applyRuntime);
}

void SubscriptionView::handleRollbackSubscription(const QString &id)
{
    if (!SubscriptionActions::rollbackSubscription(m_subscriptionService, id)) {
        QMessageBox::warning(this, tr("Notice"), tr("No config available to roll back"));
    }
}

void SubscriptionView::handleDeleteSubscription(const QString &id)
{
    if (QMessageBox::question(this, tr("Confirm"),
                              tr("Are you sure you want to delete this subscription?")) == QMessageBox::Yes) {
        m_subscriptionService->removeSubscription(id);
    }
}

void SubscriptionView::handleCopyLink(const QString &id)
{
    SubscriptionInfo target;
    if (!getSubscriptionById(id, &target)) return;
    QApplication::clipboard()->setText(target.url);
}

bool SubscriptionView::getSubscriptionById(const QString &id, SubscriptionInfo *out) const
{
    const QList<SubscriptionInfo> subs = m_subscriptionService->getSubscriptions();
    for (const auto &sub : subs) {
        if (sub.id == id) {
            if (out) *out = sub;
            return true;
        }
    }
    return false;
}

void SubscriptionView::refreshList()
{
    while (m_cardsLayout->count() > 0) {
        QLayoutItem *item = m_cardsLayout->takeAt(0);
        if (item) {
            delete item;
        }
    }
    for (SubscriptionCard *card : m_cards) {
        if (card) {
            card->deleteLater();
        }
    }
    m_cards.clear();

    const QList<SubscriptionInfo> subs = m_subscriptionService->getSubscriptions();
    const int activeIndex = m_subscriptionService->getActiveIndex();
    for (int i = 0; i < subs.count(); ++i) {
        SubscriptionCard *card = createSubscriptionCard(subs[i], i == activeIndex);
        m_cards.append(card);
    }

    layoutCards();
}

void SubscriptionView::layoutCards()
{
    if (!m_cardsLayout || !m_scrollArea || !m_cardsContainer) return;

    const int previousColumns = m_columnCount;

    QHash<QWidget*, QRect> oldGeometries;
    oldGeometries.reserve(m_cards.size());
    for (SubscriptionCard *card : std::as_const(m_cards)) {
        oldGeometries.insert(card, card->geometry());
    }

    while (m_cardsLayout->count() > 0) {
        QLayoutItem *item = m_cardsLayout->takeAt(0);
        if (item) {
            delete item;
        }
    }

    if (m_cards.isEmpty()) return;

    const int spacing = m_cardsLayout->spacing();
    const int availableWidth = qMax(0, m_scrollArea->viewport()->width());
    const int columns = CardGridLayoutHelper::computeColumns(availableWidth, spacing);
    m_columnCount = columns;
    const int horizontalMargin = CardGridLayoutHelper::computeHorizontalMargin(availableWidth, spacing, columns);
    m_cardsLayout->setContentsMargins(horizontalMargin, 0, horizontalMargin, 0);

    int row = 0;
    int col = 0;
    QList<QWidget*> widgets;
    widgets.reserve(m_cards.size());
    for (SubscriptionCard *card : m_cards) {
        card->setFixedSize(CardGridLayoutHelper::kCardWidth,
                           CardGridLayoutHelper::kCardHeight);
        m_cardsLayout->addWidget(card, row, col, Qt::AlignLeft | Qt::AlignTop);
        widgets.append(card);
        ++col;
        if (col >= columns) {
            col = 0;
            ++row;
        }
    }

    m_cardsLayout->activate();

    CardGridAnimation::animateReflow(m_cardsContainer, widgets, oldGeometries, previousColumns, columns);
}

void SubscriptionView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_cards.isEmpty()) return;

    layoutCards();
}
