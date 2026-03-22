#include "views/subscription/SubscriptionCard.h"

#include <QAction>
#include <QLabel>
#include <QPoint>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include "app/interfaces/ThemeService.h"
#include "network/SubscriptionService.h"
#include "utils/subscription/SubscriptionFormat.h"
#include "widgets/common/ElideLabel.h"
#include "widgets/common/RoundedMenu.h"

SubscriptionCard::SubscriptionCard(const SubscriptionInfo& info,
                                   bool                    active,
                                   ThemeService*           themeService,
                                   QWidget*                parent)
    : QFrame(parent),
      m_subId(info.id),
      m_active(active),
      m_updatingTimer(new QTimer(this)),
      m_themeService(themeService) {
  m_updatingTimer->setInterval(360);
  connect(m_updatingTimer, &QTimer::timeout, this, [this]() {
    m_updatingDotCount = (m_updatingDotCount % 3) + 1;
    updateActionButton();
  });
  setupUI(info);
  updateInfo(info, active);
  if (m_themeService) {
    connect(m_themeService,
            &ThemeService::themeChanged,
            this,
            &SubscriptionCard::updateStyle);
  }
}

void SubscriptionCard::setActive(bool active) {
  if (m_active == active) {
    return;
  }
  m_active = active;
  applyActiveState();
  updateStyle();
}

void SubscriptionCard::setUpdating(bool updating) {
  if (m_updating == updating) {
    return;
  }
  m_updating = updating;
  if (m_updating) {
    m_updatingDotCount = 1;
    m_updatingTimer->start();
  } else {
    m_updatingTimer->stop();
    m_updatingDotCount = 1;
  }
  updateActionButton();
}

void SubscriptionCard::setupUI(const SubscriptionInfo& info) {
  setObjectName("SubscriptionCard");
  setAttribute(Qt::WA_StyledBackground, true);
  setFrameShape(QFrame::NoFrame);
  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(18, 16, 18, 16);
  mainLayout->setSpacing(12);
  QHBoxLayout* headerLayout = new QHBoxLayout;
  headerLayout->setSpacing(10);
  m_nameLabel = new QLabel(info.name, this);
  m_nameLabel->setObjectName("CardName");
  m_typeTag = new QLabel(tr("Manual"), this);
  m_typeTag->setObjectName("CardTag");
  m_statusTag = new QLabel(this);
  m_statusTag->setAttribute(Qt::WA_StyledBackground, true);
  m_scheduleTag = new QLabel(this);
  m_scheduleTag->setObjectName("CardTagSchedule");
  QPushButton* menuBtn = new QPushButton("...");
  menuBtn->setObjectName("CardMenuBtn");
  menuBtn->setFixedSize(32, 28);
  menuBtn->setCursor(Qt::PointingHandCursor);
  RoundedMenu* menu = new RoundedMenu(this);
  menu->setObjectName("SubscriptionMenu");
  ThemeService* ts = m_themeService;
  if (ts) {
    menu->setThemeColors(ts->color("bg-secondary"), ts->color("primary"));
    connect(ts, &ThemeService::themeChanged, menu, [menu, ts]() {
      menu->setThemeColors(ts->color("bg-secondary"), ts->color("primary"));
    });
  }
  QAction* copyAction         = menu->addAction(tr("Copy Link"));

  QAction* editAction         = menu->addAction(tr("Edit Subscription"));
  m_editConfigAction          = menu->addAction(tr("Edit Current Config"));
  QAction* refreshAction      = menu->addAction(tr("Update"));
  QAction* refreshApplyAction = menu->addAction(tr("Update and Apply"));
  QAction* rollbackAction     = menu->addAction(tr("Rollback Config"));
  menu->addSeparator();
  QAction* deleteAction = menu->addAction(tr("Delete Subscription"));
  deleteAction->setObjectName("DeleteAction");
  // Manual subscriptions don't have a remote URL — hide URL-only actions
  // but keep editAction visible (renamed to "Edit Node") to reuse
  // handleEditSubscription -> NodeEditDialog flow.
  if (info.isManual) {
    editAction->setText(tr("Edit Node"));
    deleteAction->setText(tr("Delete Node"));
    copyAction->setVisible(false);
    refreshAction->setVisible(false);
    refreshApplyAction->setVisible(false);
    rollbackAction->setVisible(false);
  }
  connect(menuBtn, &QPushButton::clicked, [menuBtn, menu]() {
    menu->exec(menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
  });
  connect(copyAction, &QAction::triggered, [this]() {
    emit copyLinkClicked(m_subId);
  });

  connect(
      editAction, &QAction::triggered, [this]() { emit editClicked(m_subId); });
  connect(m_editConfigAction, &QAction::triggered, [this]() {
    emit editConfigClicked(m_subId);
  });
  connect(refreshAction, &QAction::triggered, [this]() {
    emit refreshClicked(m_subId, false);
  });
  connect(refreshApplyAction, &QAction::triggered, [this]() {
    emit refreshClicked(m_subId, true);
  });
  connect(rollbackAction, &QAction::triggered, [this]() {
    emit rollbackClicked(m_subId);
  });
  connect(deleteAction, &QAction::triggered, [this]() {
    emit deleteClicked(m_subId);
  });
  headerLayout->addWidget(m_nameLabel);
  headerLayout->addWidget(m_typeTag);
  headerLayout->addWidget(m_statusTag);
  headerLayout->addWidget(m_scheduleTag);
  headerLayout->addStretch();
  headerLayout->addWidget(menuBtn);
  QFrame* infoPanel = new QFrame(this);
  infoPanel->setObjectName("CardInfoPanel");
  QVBoxLayout* infoPanelLayout = new QVBoxLayout(infoPanel);
  infoPanelLayout->setContentsMargins(12, 10, 12, 10);
  infoPanelLayout->setSpacing(6);
  m_urlLabel = new ElideLabel(infoPanel);
  m_urlLabel->setObjectName("CardInfoText");
  m_urlLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  m_urlLabel->setMinimumWidth(0);
  m_timeLabel = new QLabel(infoPanel);
  m_timeLabel->setObjectName("CardInfoText");
  m_trafficLabel = new QLabel(infoPanel);
  m_trafficLabel->setObjectName("CardInfoText");
  m_expireLabel = new QLabel(infoPanel);
  m_expireLabel->setObjectName("CardInfoText");
  infoPanelLayout->addWidget(m_urlLabel);
  infoPanelLayout->addWidget(m_timeLabel);
  infoPanelLayout->addWidget(m_trafficLabel);
  infoPanelLayout->addWidget(m_expireLabel);
  m_useBtn = new QPushButton(this);
  m_useBtn->setCursor(Qt::PointingHandCursor);
  m_useBtn->setMinimumHeight(38);
  m_useBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  connect(
      m_useBtn, &QPushButton::clicked, [this]() { emit useClicked(m_subId); });
  mainLayout->addLayout(headerLayout);
  mainLayout->addWidget(infoPanel);
  mainLayout->addStretch();
  mainLayout->addWidget(m_useBtn);
}

void SubscriptionCard::applyActiveState() {
  if (m_statusTag) {
    m_statusTag->setText(m_active ? tr("Active") : tr("Inactive"));
    m_statusTag->setObjectName(m_active ? "CardTagActive" : "CardTag");
  }
  updateActionButton();
  if (m_editConfigAction) {
    m_editConfigAction->setVisible(m_active);
  }
}

void SubscriptionCard::updateActionButton() {
  if (!m_useBtn) {
    return;
  }
  m_useBtn->setObjectName(m_active ? "CardActionBtnActive" : "CardActionBtn");
  if (m_updating) {
    m_useBtn->setText(tr("updating%1").arg(QString(m_updatingDotCount, '.')));
    return;
  }
  m_useBtn->setText(m_active ? tr("Refresh") : tr("Use"));
}

void SubscriptionCard::updateInfo(const SubscriptionInfo& info, bool active) {
  m_subId = info.id;
  if (m_nameLabel) {
    m_nameLabel->setText(info.name);
  }
  if (m_typeTag) {
    m_typeTag->setVisible(info.isManual);
  }
  if (m_scheduleTag) {
    if (!info.isManual && info.autoUpdateIntervalMinutes > 0) {
      const int intervalMinutes = info.autoUpdateIntervalMinutes;
      if (intervalMinutes % 60 == 0) {
        m_scheduleTag->setText(tr("Every %1 hours").arg(intervalMinutes / 60));
      } else {
        m_scheduleTag->setText(tr("Every %1 minutes").arg(intervalMinutes));
      }
      m_scheduleTag->setVisible(true);
    } else {
      m_scheduleTag->setVisible(false);
    }
  }
  m_urlLabel->setFullText(info.isManual ? tr("Manual config content") : info.url);
  if (m_timeLabel) {
    m_timeLabel->setText(tr("Updated: ") +
                         SubscriptionFormat::formatTimestamp(info.lastUpdate));
  }
  if (m_trafficLabel) {
    if (info.subscriptionUpload >= 0 || info.subscriptionDownload >= 0) {
      qint64 used = qMax<qint64>(0, info.subscriptionUpload) +
                    qMax<qint64>(0, info.subscriptionDownload);
      QString trafficText;
      if (info.subscriptionTotal > 0) {
        qint64 remain = qMax<qint64>(0, info.subscriptionTotal - used);
        trafficText =
            tr("Used %1 / Total %2 / Remaining %3")
                .arg(SubscriptionFormat::formatBytes(used))
                .arg(SubscriptionFormat::formatBytes(info.subscriptionTotal))
                .arg(SubscriptionFormat::formatBytes(remain));
      } else {
        trafficText = tr("Used %1").arg(SubscriptionFormat::formatBytes(used));
      }
      m_trafficLabel->setText(tr("Traffic: ") + trafficText);
      m_trafficLabel->setVisible(true);
    } else {
      m_trafficLabel->setVisible(false);
    }
  }
  if (m_expireLabel) {
    if (info.subscriptionExpire > 0) {
      m_expireLabel->setText(
          tr("Expires: ") +
          SubscriptionFormat::formatExpireTime(info.subscriptionExpire));
      m_expireLabel->setVisible(true);
    } else {
      m_expireLabel->setVisible(false);
    }
  }
  m_active = active;
  applyActiveState();
  updateStyle();
}

void SubscriptionCard::updateStyle() {
  ThemeService* ts = m_themeService;
  if (!ts) {
    return;
  }
  QString qss = ts->loadStyleSheet(":/styles/card_common.qss");
  setStyleSheet(qss);
}
