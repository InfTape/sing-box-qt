
#include "SubscriptionView.h"
#include "dialogs/ConfigEditDialog.h"
#include "dialogs/SubscriptionFormDialog.h"
#include "network/SubscriptionService.h"
#include "utils/ThemeManager.h"
#include "views/subscription/SubscriptionCard.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>

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

    refreshList();
}

void SubscriptionView::updateStyle()
{
    ThemeManager &tm = ThemeManager::instance();
    setStyleSheet(tm.loadStyleSheet(":/styles/subscription_view.qss"));

    if (m_addBtn) {
        QColor primary = tm.getColor("primary");
        QColor bg = primary; bg.setAlphaF(0.2);
        QColor border = primary; border.setAlphaF(0.4);
        QColor hover = primary; hover.setAlphaF(0.3);
        
        m_addBtn->setStyleSheet(QString(
            "QPushButton { background-color: %1; color: %2; border: 1px solid %3; "
            "border-radius: 10px; padding: 8px 16px; font-weight: bold; font-size: 13px; }"
            "QPushButton:hover { background-color: %4; }"
        ).arg(bg.name(QColor::HexArgb), primary.name(), border.name(QColor::HexArgb), hover.name(QColor::HexArgb)));
    }
}

void SubscriptionView::onAddClicked()
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
        m_subscriptionService->addManualSubscription(content, dialog.name(), useOriginal, dialog.isUriList(), true);
    } else {
        m_subscriptionService->addUrlSubscription(dialog.url(), dialog.name(), useOriginal, interval, true);
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
    m_subscriptionService->setActiveSubscription(id, true);
}

void SubscriptionView::handleEditSubscription(const QString &id)
{
    SubscriptionInfo target;
    if (!getSubscriptionById(id, &target)) return;

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
        dialog.autoUpdateIntervalMinutes()
    );
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
    m_subscriptionService->refreshSubscription(id, applyRuntime);
}

void SubscriptionView::handleRollbackSubscription(const QString &id)
{
    SubscriptionInfo target;
    if (!getSubscriptionById(id, &target)) return;

    if (!m_subscriptionService->rollbackSubscriptionConfig(target.configPath)) {
        QMessageBox::warning(this, tr("Notice"), tr("No config available to roll back"));
        return;
    }
    if (m_subscriptionService->getActiveIndex() >= 0) {
        m_subscriptionService->setActiveSubscription(id, true);
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

    while (m_cardsLayout->count() > 0) {
        QLayoutItem *item = m_cardsLayout->takeAt(0);
        if (item) {
            delete item;
        }
    }

    if (m_cards.isEmpty()) return;

    const int availableWidth = m_scrollArea->viewport()->width();
    const int spacing = m_cardsLayout->spacing();
    const int minColumns = 2;
    const int maxColumns = 4;
    const int idealCardWidth = 320;
    int columns = availableWidth / idealCardWidth;
    columns = qMax(minColumns, qMin(columns, maxColumns));
    m_columnCount = columns;
    const int totalSpacing = spacing * (columns - 1);
    const int cardWidth = qMax(0, (availableWidth - totalSpacing) / columns);
    const int cardHeight = qMax(200, qRound(cardWidth * 0.68));

    int row = 0;
    int col = 0;
    for (SubscriptionCard *card : m_cards) {
        card->setFixedSize(cardWidth, cardHeight);
        m_cardsLayout->addWidget(card, row, col);
        ++col;
        if (col >= columns) {
            col = 0;
            ++row;
        }
    }
    for (int i = 0; i < columns; ++i) {
        m_cardsLayout->setColumnStretch(i, 1);
    }
}

void SubscriptionView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_cards.isEmpty()) return;

    const int availableWidth = m_scrollArea->viewport()->width();
    const int spacing = m_cardsLayout->spacing();
    const int minColumns = 2;
    const int maxColumns = 4;
    const int idealCardWidth = 320;
    int columns = availableWidth / idealCardWidth;
    columns = qMax(minColumns, qMin(columns, maxColumns));
    if (columns != m_columnCount) {
        layoutCards();
        return;
    }

    const int totalSpacing = spacing * (columns - 1);
    const int cardWidth = qMax(0, (availableWidth - totalSpacing) / columns);
    const int cardHeight = qMax(200, qRound(cardWidth * 0.68));
    for (int i = 0; i < m_cardsLayout->count(); ++i) {
        if (QLayoutItem *item = m_cardsLayout->itemAt(i)) {
            if (QWidget *widget = item->widget()) {
                widget->setFixedSize(cardWidth, cardHeight);
            }
        }
    }
}
