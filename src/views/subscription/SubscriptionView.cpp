#include "SubscriptionView.h"

#include <QAbstractAnimation>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QtGlobal>

#include "app/interfaces/ThemeService.h"
#include "dialogs/config/ConfigEditDialog.h"
#include "dialogs/subscription/NodeEditDialog.h"
#include "dialogs/subscription/SubscriptionFormDialog.h"
#include "network/SubscriptionService.h"
#include "utils/layout/CardGridAnimation.h"
#include "utils/layout/CardGridLayoutHelper.h"
#include "utils/subscription/SubscriptionActions.h"
#include "utils/subscription/SubscriptionHelpers.h"
#include "views/subscription/SubscriptionCard.h"
#include "views/subscription/SubscriptionController.h"
#include "widgets/common/RoundedMenu.h"
// ==================== SubscriptionView ====================

SubscriptionView::SubscriptionView(SubscriptionService* service,
                                   ThemeService* themeService, QWidget* parent)
    : QWidget(parent),
      m_subscriptionService(service),
      m_subscriptionController(new SubscriptionController(service)),
      m_autoUpdateTimer(new QTimer(this)),
      m_themeService(themeService) {
  Q_ASSERT(m_subscriptionService);
  setupUI();

  if (m_themeService) {
    connect(m_themeService, &ThemeService::themeChanged, this,
            &SubscriptionView::updateStyle);
  }
  updateStyle();

  m_autoUpdateTimer->setInterval(30 * 60 * 1000);
  connect(m_autoUpdateTimer, &QTimer::timeout, this,
          &SubscriptionView::onAutoUpdateTimer);
  m_autoUpdateTimer->start();
}
SubscriptionService* SubscriptionView::getService() const {
  return m_subscriptionService;
}
void SubscriptionView::setupUI() {
  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(24, 24, 24, 24);
  mainLayout->setSpacing(16);

  QHBoxLayout* headerLayout = new QHBoxLayout;
  QVBoxLayout* titleLayout = new QVBoxLayout;

  QLabel* titleLabel = new QLabel(tr("Subscription Manager"));
  titleLabel->setObjectName("PageTitle");

  QLabel* subtitleLabel =
      new QLabel(tr("Manage your subscriptions and proxy nodes"));
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
  m_scrollArea->viewport()->installEventFilter(this);

  mainLayout->addWidget(m_scrollArea, 1);

  connect(m_addBtn, &QPushButton::clicked, this,
          &SubscriptionView::onAddClicked);

  connect(m_subscriptionService, &SubscriptionService::subscriptionAdded, this,
          &SubscriptionView::refreshList);
  connect(m_subscriptionService, &SubscriptionService::subscriptionRemoved,
          this, &SubscriptionView::refreshList);
  connect(m_subscriptionService, &SubscriptionService::subscriptionUpdated,
          this, &SubscriptionView::handleSubscriptionUpdated);
  connect(m_subscriptionService,
          &SubscriptionService::activeSubscriptionChanged, this,
          &SubscriptionView::handleActiveSubscriptionChanged);
  connect(m_subscriptionService, &SubscriptionService::errorOccurred, this,
          [this](const QString& err) {
            QMessageBox::warning(this, tr("Notice"), err);
          });

  refreshList();
}
void SubscriptionView::updateStyle() {
  ThemeService* ts = m_themeService;
  if (ts) setStyleSheet(ts->loadStyleSheet(":/styles/subscription_view.qss"));
}
void SubscriptionView::onAddClicked() {
  RoundedMenu menu(this);
  menu.setObjectName("TrayMenu");
  ThemeService* ts = m_themeService;
  if (ts) {
    menu.setThemeColors(ts->color("bg-secondary"), ts->color("primary"));
    connect(ts, &ThemeService::themeChanged, &menu, [&menu, ts]() {
      menu.setThemeColors(ts->color("bg-secondary"), ts->color("primary"));
    });
  }
  menu.addAction(tr("Add subs url"), this,
                 &SubscriptionView::openSubscriptionDialog);
  menu.addAction(tr("Manual add node"), this,
                 &SubscriptionView::onAddNodeClicked);
  menu.setMinimumWidth(m_addBtn->width());
  menu.exec(m_addBtn->mapToGlobal(QPoint(0, m_addBtn->height())));
}
void SubscriptionView::openSubscriptionDialog() {
  SubscriptionFormDialog dialog(m_themeService, this);
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
    const QString content =
        dialog.isUriList() ? dialog.uriContent() : dialog.manualContent();
    m_subscriptionController->addManual(
        content, dialog.name(), useOriginal, dialog.isUriList(), true,
        dialog.sharedRulesEnabled(), dialog.ruleSets());
  } else {
    m_subscriptionController->addUrl(
        dialog.url(), dialog.name(), useOriginal, interval, true,
        dialog.sharedRulesEnabled(), dialog.ruleSets());
  }
}
void SubscriptionView::onAddNodeClicked() {
  NodeEditDialog dialog(m_themeService, this);
  if (dialog.exec() == QDialog::Accepted) {
    QJsonObject node = dialog.nodeData();
    QJsonArray arr;
    arr.append(node);
    QJsonDocument doc(arr);
    QString content = doc.toJson(QJsonDocument::Compact);

    QString name = node["tag"].toString();
    m_subscriptionController->addManual(content, name, false, false, true,
                                        dialog.sharedRulesEnabled(),
                                        dialog.ruleSets());
  }
}
void SubscriptionView::onAutoUpdateTimer() {
  const QList<SubscriptionInfo> subs =
      m_subscriptionController->subscriptions();
  const int activeIndex = m_subscriptionController->activeIndex();
  const qint64 now = QDateTime::currentMSecsSinceEpoch();

  for (int i = 0; i < subs.count(); ++i) {
    const SubscriptionInfo& item = subs[i];
    if (item.isManual) continue;
    const int interval = item.autoUpdateIntervalMinutes;
    if (interval <= 0) continue;
    if (item.lastUpdate <= 0) continue;
    if (now - item.lastUpdate >= static_cast<qint64>(interval) * 60 * 1000) {
      m_subscriptionController->refresh(item.id, i == activeIndex);
    }
  }
}
SubscriptionCard* SubscriptionView::createSubscriptionCard(
    const SubscriptionInfo& info, bool active) {
  QWidget* parent = m_cardsContainer ? m_cardsContainer : this;
  SubscriptionCard* card =
      new SubscriptionCard(info, active, m_themeService, parent);
  wireCardSignals(card);
  return card;
}
void SubscriptionView::wireCardSignals(SubscriptionCard* card) {
  connect(card, &SubscriptionCard::useClicked, this,
          &SubscriptionView::handleUseSubscription);
  connect(card, &SubscriptionCard::editClicked, this,
          &SubscriptionView::handleEditSubscription);
  connect(card, &SubscriptionCard::editConfigClicked, this,
          &SubscriptionView::handleEditConfig);
  connect(card, &SubscriptionCard::refreshClicked, this,
          &SubscriptionView::handleRefreshSubscription);
  connect(card, &SubscriptionCard::rollbackClicked, this,
          &SubscriptionView::handleRollbackSubscription);
  connect(card, &SubscriptionCard::deleteClicked, this,
          &SubscriptionView::handleDeleteSubscription);
  connect(card, &SubscriptionCard::copyLinkClicked, this,
          &SubscriptionView::handleCopyLink);
}
void SubscriptionView::handleUseSubscription(const QString& id) {
  SubscriptionActions::useSubscription(m_subscriptionController->service(), id);
}
void SubscriptionView::handleEditSubscription(const QString& id) {
  SubscriptionInfo target;
  if (!getSubscriptionById(id, &target)) return;

  QJsonObject singleNodeObj;
  const bool isSingleNode =
      SubscriptionHelpers::isSingleManualNode(target, &singleNodeObj);

  if (isSingleNode) {
    NodeEditDialog dialog(m_themeService, this);
    dialog.setRuleSets(target.ruleSets, target.enableSharedRules);
    dialog.setNodeData(singleNodeObj);
    if (dialog.exec() != QDialog::Accepted) return;

    QJsonObject newNode = dialog.nodeData();
    QJsonArray arr;
    arr.append(newNode);
    QString content = QJsonDocument(arr).toJson(QJsonDocument::Compact);
    QString name = newNode["tag"].toString();

    m_subscriptionController->updateSubscription(
        id, name, target.url,
        true,  // isManual
        content, target.useOriginalConfig, target.autoUpdateIntervalMinutes,
        dialog.sharedRulesEnabled(), dialog.ruleSets());
  } else {
    SubscriptionFormDialog dialog(m_themeService, this);
    dialog.setEditData(target);
    if (dialog.exec() != QDialog::Accepted) return;

    QString error;
    if (!dialog.validateInput(&error)) {
      QMessageBox::warning(this, tr("Notice"), error);
      return;
    }

    const bool isManual = dialog.isManual();
    const QString content =
        dialog.isUriList() ? dialog.uriContent() : dialog.manualContent();
    m_subscriptionController->updateSubscription(
        id, dialog.name(), dialog.url(), isManual, content,
        dialog.useOriginalConfig(), dialog.autoUpdateIntervalMinutes(),
        dialog.sharedRulesEnabled(), dialog.ruleSets());
  }
}
void SubscriptionView::handleEditConfig(const QString& id) {
  Q_UNUSED(id)
  const QString current = m_subscriptionController->currentConfig();
  if (current.isEmpty()) {
    QMessageBox::warning(this, tr("Notice"), tr("Current config not found"));
    return;
  }
  ConfigEditDialog dialog(this);
  dialog.setContent(current);
  if (dialog.exec() == QDialog::Accepted) {
    if (!m_subscriptionController->saveCurrentConfig(dialog.content(), true)) {
      QMessageBox::warning(this, tr("Notice"), tr("Failed to save config"));
    }
  }
}
void SubscriptionView::handleRefreshSubscription(const QString& id,
                                                 bool applyRuntime) {
  SubscriptionActions::refreshSubscription(m_subscriptionController->service(),
                                           id, applyRuntime);
}
void SubscriptionView::handleRollbackSubscription(const QString& id) {
  if (!SubscriptionActions::rollbackSubscription(
          m_subscriptionController->service(), id)) {
    QMessageBox::warning(this, tr("Notice"),
                         tr("No config available to roll back"));
  }
}
void SubscriptionView::handleDeleteSubscription(const QString& id) {
  if (QMessageBox::question(
          this, tr("Confirm"),
          tr("Are you sure you want to delete this subscription?")) ==
      QMessageBox::Yes) {
    m_subscriptionController->remove(id);
  }
}
void SubscriptionView::handleCopyLink(const QString& id) {
  SubscriptionInfo target;
  if (!getSubscriptionById(id, &target)) return;
  QApplication::clipboard()->setText(target.url);
}
void SubscriptionView::handleSubscriptionUpdated(const QString& id) {
  SubscriptionInfo target;
  if (!getSubscriptionById(id, &target)) {
    refreshList();
    return;
  }
  SubscriptionCard* card = findCardById(id);
  if (!card) {
    refreshList();
    return;
  }

  QString activeId;
  const QList<SubscriptionInfo> subs =
      m_subscriptionController->subscriptions();
  const int activeIndex = m_subscriptionController->activeIndex();
  if (activeIndex >= 0 && activeIndex < subs.count()) {
    activeId = subs[activeIndex].id;
  }
  card->updateInfo(target, !activeId.isEmpty() && activeId == id);
}
void SubscriptionView::handleActiveSubscriptionChanged(
    const QString& id, const QString& configPath) {
  Q_UNUSED(configPath)
  updateActiveCards(id);
}
SubscriptionCard* SubscriptionView::findCardById(const QString& id) const {
  for (SubscriptionCard* card : m_cards) {
    if (card && card->subscriptionId() == id) {
      return card;
    }
  }
  return nullptr;
}
void SubscriptionView::updateActiveCards(const QString& activeId) {
  for (SubscriptionCard* card : m_cards) {
    if (!card) continue;
    const bool isActive =
        !activeId.isEmpty() && card->subscriptionId() == activeId;
    card->setActive(isActive);
  }
}
bool SubscriptionView::getSubscriptionById(const QString& id,
                                           SubscriptionInfo* out) const {
  const QList<SubscriptionInfo> subs =
      m_subscriptionController->subscriptions();
  for (const auto& sub : subs) {
    if (sub.id == id) {
      if (out) *out = sub;
      return true;
    }
  }
  return false;
}
void SubscriptionView::refreshList() {
  while (m_cardsLayout->count() > 0) {
    QLayoutItem* item = m_cardsLayout->takeAt(0);
    if (!item) continue;
    if (QWidget* w = item->widget()) {
      w->hide();
      w->deleteLater();
    }
    delete item;
  }
  m_cards.clear();

  const QList<SubscriptionInfo> subs =
      m_subscriptionController->subscriptions();
  const int activeIndex = m_subscriptionController->activeIndex();
  for (int i = 0; i < subs.count(); ++i) {
    SubscriptionCard* card = createSubscriptionCard(subs[i], i == activeIndex);
    m_cards.append(card);
  }

  m_skipNextAnimation = true;
  QTimer::singleShot(0, this, [this]() { layoutCards(); });
}
void SubscriptionView::layoutCards() {
  if (!m_cardsLayout || !m_scrollArea || !m_cardsContainer) return;

  const int previousColumns = m_columnCount;

  const auto runningAnimations =
      m_cardsContainer->findChildren<QAbstractAnimation*>();
  for (QAbstractAnimation* anim : runningAnimations) {
    if (!anim) continue;
    anim->stop();
    anim->deleteLater();
  }

  QHash<QWidget*, QRect> oldGeometries;
  oldGeometries.reserve(m_cards.size());
  for (SubscriptionCard* card : std::as_const(m_cards)) {
    oldGeometries.insert(card, card->geometry());
  }

  while (m_cardsLayout->count() > 0) {
    QLayoutItem* item = m_cardsLayout->takeAt(0);
    if (item) {
      delete item;
    }
  }

  if (m_cards.isEmpty()) return;

  const int spacing = m_cardsLayout->spacing();
  const int availableWidth = qMax(0, m_scrollArea->viewport()->width());
  const int columns =
      CardGridLayoutHelper::computeColumns(availableWidth, spacing);
  m_columnCount = columns;
  const int horizontalMargin = CardGridLayoutHelper::computeHorizontalMargin(
      availableWidth, spacing, columns);
  m_cardsLayout->setContentsMargins(horizontalMargin, 0, horizontalMargin, 0);

  int row = 0;
  int col = 0;
  QList<QWidget*> widgets;
  widgets.reserve(m_cards.size());
  for (SubscriptionCard* card : m_cards) {
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

  if (m_skipNextAnimation) {
    m_skipNextAnimation = false;
  } else {
    CardGridAnimation::animateReflow(m_cardsContainer, widgets, oldGeometries,
                                     previousColumns, columns);
  }
}
void SubscriptionView::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  if (m_cards.isEmpty()) return;

  layoutCards();
}
bool SubscriptionView::eventFilter(QObject* watched, QEvent* event) {
  if (m_scrollArea && watched == m_scrollArea->viewport() &&
      event->type() == QEvent::Resize) {
    if (!m_cards.isEmpty()) layoutCards();
  }
  return QWidget::eventFilter(watched, event);
}
void SubscriptionView::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  if (m_cards.isEmpty()) return;
  m_skipNextAnimation = true;
  QTimer::singleShot(0, this, [this]() {
    if (!m_cards.isEmpty()) layoutCards();
  });
}
