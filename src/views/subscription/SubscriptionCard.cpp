#include "views/subscription/SubscriptionCard.h"
#include "network/SubscriptionService.h"
#include "utils/SubscriptionFormat.h"
#include "utils/ThemeManager.h"
#include "widgets/RoundedMenu.h"
#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QPoint>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QStyle>
#include <QtMath>

SubscriptionCard::SubscriptionCard(const SubscriptionInfo &info, bool active, QWidget *parent)
    : QFrame(parent)
    , m_subId(info.id)
    , m_active(active)
{
    setupUI(info);
    applyActiveState();
    updateStyle();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &SubscriptionCard::updateStyle);
}

void SubscriptionCard::setActive(bool active)
{
    if (m_active == active) return;
    m_active = active;
    applyActiveState();
    updateStyle();
}

void SubscriptionCard::setupUI(const SubscriptionInfo &info)
{
    setObjectName("SubscriptionCard");
    setFrameShape(QFrame::NoFrame);
    setFixedHeight(190);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 12, 16, 12);
    mainLayout->setSpacing(8);

    QHBoxLayout *headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(10);

    QLabel *iconLabel = new QLabel(tr("LINK"));
    iconLabel->setObjectName("CardIcon");
    iconLabel->setFixedSize(44, 32);
    iconLabel->setAlignment(Qt::AlignCenter);

    QLabel *nameLabel = new QLabel(info.name);
    nameLabel->setObjectName("CardName");

    QLabel *typeTag = new QLabel(info.isManual ? tr("Manual Config") : tr("Subscription URL"));
    typeTag->setObjectName("CardTag");

    m_statusTag = new QLabel(this);
    m_statusTag->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QLabel *scheduleTag = new QLabel(tr("Every %1 minutes").arg(info.autoUpdateIntervalMinutes));
    scheduleTag->setObjectName("CardTagSchedule");
    scheduleTag->setVisible(!info.isManual && info.autoUpdateIntervalMinutes > 0);

    QPushButton *menuBtn = new QPushButton("...");
    menuBtn->setObjectName("CardMenuBtn");
    menuBtn->setFixedSize(32, 28);
    menuBtn->setCursor(Qt::PointingHandCursor);

    RoundedMenu *menu = new RoundedMenu(this);
    menu->setObjectName("SubscriptionMenu");
    ThemeManager &tm = ThemeManager::instance();
    menu->setThemeColors(tm.getColor("bg-secondary"), tm.getColor("primary"));
    connect(&tm, &ThemeManager::themeChanged, menu, [menu, &tm]() {
        menu->setThemeColors(tm.getColor("bg-secondary"), tm.getColor("primary"));
    });
    QAction *copyAction = menu->addAction(tr("Copy Link"));
    QAction *editAction = menu->addAction(tr("Edit Subscription"));
    m_editConfigAction = menu->addAction(tr("Edit Current Config"));
    QAction *refreshAction = menu->addAction(tr("Refresh Now"));
    QAction *refreshApplyAction = menu->addAction(tr("Refresh and Apply"));
    QAction *rollbackAction = menu->addAction(tr("Rollback Config"));
    menu->addSeparator();
    QAction *deleteAction = menu->addAction(tr("Delete Subscription"));
    deleteAction->setObjectName("DeleteAction");

    connect(menuBtn, &QPushButton::clicked, [menuBtn, menu]() {
        menu->exec(menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
    });

    connect(copyAction, &QAction::triggered, [this]() { emit copyLinkClicked(m_subId); });
    connect(editAction, &QAction::triggered, [this]() { emit editClicked(m_subId); });
    connect(m_editConfigAction, &QAction::triggered, [this]() { emit editConfigClicked(m_subId); });
    connect(refreshAction, &QAction::triggered, [this]() { emit refreshClicked(m_subId, false); });
    connect(refreshApplyAction, &QAction::triggered, [this]() { emit refreshClicked(m_subId, true); });
    connect(rollbackAction, &QAction::triggered, [this]() { emit rollbackClicked(m_subId); });
    connect(deleteAction, &QAction::triggered, [this]() { emit deleteClicked(m_subId); });

    headerLayout->addWidget(iconLabel);
    headerLayout->addWidget(nameLabel);
    headerLayout->addWidget(typeTag);
    headerLayout->addWidget(m_statusTag);
    headerLayout->addWidget(scheduleTag);
    headerLayout->addStretch();
    headerLayout->addWidget(menuBtn);

    QHBoxLayout *infoLayout = new QHBoxLayout;
    infoLayout->setSpacing(16);

    QString urlText = info.isManual ? tr("Manual config content") : info.url;
    if (urlText.length() > 45) {
        urlText = urlText.left(45) + "...";
    }
    QLabel *urlLabel = new QLabel(tr("URL: ") + urlText);
    urlLabel->setObjectName("CardInfoText");

    QLabel *timeLabel = new QLabel(tr("Updated: ") + SubscriptionFormat::formatTimestamp(info.lastUpdate));
    timeLabel->setObjectName("CardInfoText");

    infoLayout->addWidget(urlLabel, 1);
    infoLayout->addWidget(timeLabel);

    QVBoxLayout *metaLayout = new QVBoxLayout;
    if (info.subscriptionUpload >= 0 || info.subscriptionDownload >= 0) {
        qint64 used = qMax<qint64>(0, info.subscriptionUpload) + qMax<qint64>(0, info.subscriptionDownload);
        QString trafficText;
        if (info.subscriptionTotal > 0) {
            qint64 remain = qMax<qint64>(0, info.subscriptionTotal - used);
            trafficText = tr("Used %1 / Total %2 / Remaining %3")
                .arg(SubscriptionFormat::formatBytes(used))
                .arg(SubscriptionFormat::formatBytes(info.subscriptionTotal))
                .arg(SubscriptionFormat::formatBytes(remain));
        } else {
            trafficText = tr("Used %1").arg(SubscriptionFormat::formatBytes(used));
        }
        QLabel *trafficLabel = new QLabel(tr("Traffic: ") + trafficText);
        trafficLabel->setObjectName("CardInfoText");
        metaLayout->addWidget(trafficLabel);
    }

    if (info.subscriptionExpire > 0) {
        QLabel *expireLabel = new QLabel(tr("Expires: ") + SubscriptionFormat::formatExpireTime(info.subscriptionExpire));
        expireLabel->setObjectName("CardInfoText");
        metaLayout->addWidget(expireLabel);
    }

    m_useBtn = new QPushButton(this);
    m_useBtn->setObjectName("CardActionBtn");
    m_useBtn->setCursor(Qt::PointingHandCursor);
    m_useBtn->setFixedHeight(36);
    m_useBtn->setFixedWidth(110);
    connect(m_useBtn, &QPushButton::clicked, [this]() { emit useClicked(m_subId); });

    mainLayout->addLayout(headerLayout);
    mainLayout->addLayout(infoLayout);
    if (metaLayout->count() > 0) {
        mainLayout->addLayout(metaLayout);
    }
    mainLayout->addStretch();
    mainLayout->addWidget(m_useBtn);
}

void SubscriptionCard::applyActiveState()
{
    if (m_statusTag) {
        m_statusTag->setText(m_active ? tr("Active") : tr("Inactive"));
        m_statusTag->setObjectName(m_active ? "CardTagActive" : "CardTag");
        m_statusTag->style()->unpolish(m_statusTag);
        m_statusTag->style()->polish(m_statusTag);
    }
    if (m_useBtn) {
        m_useBtn->setText(m_active ? tr("Use Again") : tr("Use"));
    }
    if (m_editConfigAction) {
        m_editConfigAction->setVisible(m_active);
    }
}

void SubscriptionCard::updateStyle()
{
    ThemeManager &tm = ThemeManager::instance();

    QMap<QString, QString> extra;
    extra.insert("card-bg", m_active ? tm.getColorString("bg-tertiary") : tm.getColorString("bg-secondary"));
    extra.insert("card-border", m_active ? tm.getColorString("primary") : tm.getColorString("border"));
    setStyleSheet(tm.loadStyleSheet(":/styles/subscription_card.qss", extra));
}
