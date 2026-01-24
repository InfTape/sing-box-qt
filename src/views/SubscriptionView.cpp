
#include "SubscriptionView.h"
#include "network/SubscriptionService.h"
#include "utils/ThemeManager.h"
#include "widgets/RoundedMenu.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QDialog>
#include <QTabWidget>
#include <QTextEdit>
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonParseError>

namespace {
QString formatBytes(qint64 bytes)
{
    if (bytes <= 0) return "0 B";
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int index = 0;
    while (value >= 1024.0 && index < 4) {
        value /= 1024.0;
        index++;
    }
    return QString::number(value, 'f', 2) + " " + units[index];
}

QString formatTimestamp(qint64 ms)
{
    if (ms <= 0) return QObject::tr("Never updated");
    return QDateTime::fromMSecsSinceEpoch(ms).toString("yyyy-MM-dd HH:mm:ss");
}

QString formatExpireTime(qint64 seconds)
{
    if (seconds <= 0) return QString();
    return QObject::tr("Expires: %1").arg(QDateTime::fromSecsSinceEpoch(seconds).toString("yyyy-MM-dd HH:mm"));
}

bool isJsonText(const QString &text)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &err);
    return err.error == QJsonParseError::NoError && doc.isObject();
}

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

class SubscriptionFormDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SubscriptionFormDialog(QWidget *parent = nullptr)
        : QDialog(parent)
        , m_nameEdit(new QLineEdit)
        , m_tabs(new QTabWidget)
        , m_urlEdit(new QTextEdit)
        , m_manualEdit(new QTextEdit)
        , m_uriEdit(new QTextEdit)
        , m_useOriginalCheck(new QCheckBox(tr("Use original config")))
        , m_autoUpdateCombo(new MenuComboBox)
        , m_hintLabel(new QLabel)
    {
        setWindowTitle(tr("Subscription Manager"));
        setModal(true);
        setMinimumWidth(520);

        QVBoxLayout *layout = new QVBoxLayout(this);

        QFormLayout *formLayout = new QFormLayout;
        formLayout->addRow(tr("Name"), m_nameEdit);

        m_tabs->addTab(createTextTab(m_urlEdit, tr("Subscription URL")), tr("Link"));
        m_tabs->addTab(createTextTab(m_manualEdit, tr("JSON Config")), tr("Manual Config"));
        m_tabs->addTab(createTextTab(m_uriEdit, tr("URI List")), tr("URI List"));

        m_autoUpdateCombo->addItem(tr("Off"), 0);
        m_autoUpdateCombo->addItem(tr("6 hours"), 360);
        m_autoUpdateCombo->addItem(tr("12 hours"), 720);
        m_autoUpdateCombo->addItem(tr("1 day"), 1440);

        m_hintLabel->setWordWrap(true);
        m_hintLabel->setStyleSheet("color: #f59e0b; font-size: 12px;");
        m_hintLabel->setText(tr("Advanced templates are disabled when using the original config"));
        m_hintLabel->setVisible(false);

        layout->addLayout(formLayout);
        layout->addWidget(m_tabs);
        layout->addWidget(m_useOriginalCheck);
        layout->addWidget(m_hintLabel);
        QLabel *autoUpdateLabel = new QLabel(tr("Auto update"));
        layout->addWidget(autoUpdateLabel);
        layout->addWidget(m_autoUpdateCombo);

        QHBoxLayout *actions = new QHBoxLayout;
        QPushButton *cancelBtn = new QPushButton(tr("Cancel"));
        QPushButton *okBtn = new QPushButton(tr("OK"));
        actions->addStretch();
        actions->addWidget(cancelBtn);
        actions->addWidget(okBtn);
        layout->addLayout(actions);

        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        connect(m_tabs, &QTabWidget::currentChanged, this, &SubscriptionFormDialog::updateState);
        connect(m_useOriginalCheck, &QCheckBox::toggled, this, &SubscriptionFormDialog::updateState);

        updateState();
    }

    void setEditData(const SubscriptionInfo &info)
    {
        m_nameEdit->setText(info.name);
        if (info.isManual) {
            if (isJsonText(info.manualContent)) {
                m_tabs->setCurrentIndex(1);
                m_manualEdit->setPlainText(info.manualContent);
            } else {
                m_tabs->setCurrentIndex(2);
                m_uriEdit->setPlainText(info.manualContent);
            }
        } else {
            m_tabs->setCurrentIndex(0);
            m_urlEdit->setPlainText(info.url);
        }
        m_useOriginalCheck->setChecked(info.useOriginalConfig);
        m_autoUpdateCombo->setCurrentIndex(indexForInterval(info.autoUpdateIntervalMinutes));
        updateState();
    }

    QString name() const { return m_nameEdit->text().trimmed(); }
    QString url() const { return m_urlEdit->toPlainText().trimmed(); }
    QString manualContent() const { return m_manualEdit->toPlainText().trimmed(); }
    QString uriContent() const { return m_uriEdit->toPlainText().trimmed(); }
    bool isManual() const { return m_tabs->currentIndex() != 0; }
    bool isUriList() const { return m_tabs->currentIndex() == 2; }
    bool useOriginalConfig() const { return m_useOriginalCheck->isChecked(); }
    int autoUpdateIntervalMinutes() const { return m_autoUpdateCombo->currentData().toInt(); }

    bool validateInput(QString *error) const
    {
        if (name().isEmpty()) {
            if (error) *error = tr("Please enter a subscription name");
            return false;
        }

        if (m_tabs->currentIndex() == 0 && url().isEmpty()) {
            if (error) *error = tr("Please enter a subscription URL");
            return false;
        }
        if (m_tabs->currentIndex() == 1 && manualContent().isEmpty()) {
            if (error) *error = tr("Please enter subscription content");
            return false;
        }
        if (m_tabs->currentIndex() == 2 && uriContent().isEmpty()) {
            if (error) *error = tr("Please enter URI content");
            return false;
        }

        if (useOriginalConfig() && m_tabs->currentIndex() != 0) {
            QString content = m_tabs->currentIndex() == 1 ? manualContent() : uriContent();
            if (!isJsonText(content)) {
                if (error) *error = tr("Original subscription only supports sing-box JSON config");
                return false;
            }
        }
        return true;
    }

private:
    QWidget* createTextTab(QTextEdit *edit, const QString &placeholder)
    {
        QWidget *widget = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(widget);
        edit->setPlaceholderText(placeholder + "...");
        edit->setMinimumHeight(110);
        layout->addWidget(edit);
        return widget;
    }

    int indexForInterval(int minutes) const
    {
        for (int i = 0; i < m_autoUpdateCombo->count(); ++i) {
            if (m_autoUpdateCombo->itemData(i).toInt() == minutes) {
                return i;
            }
        }
        return 0;
    }

    void updateState()
    {
        bool urlTab = m_tabs->currentIndex() == 0;
        m_autoUpdateCombo->setEnabled(urlTab);
        if (!urlTab) {
            m_autoUpdateCombo->setCurrentIndex(0);
        }
        bool showHint = m_useOriginalCheck->isChecked();
        m_hintLabel->setVisible(showHint);
    }

    QLineEdit *m_nameEdit;
    QTabWidget *m_tabs;
    QTextEdit *m_urlEdit;
    QTextEdit *m_manualEdit;
    QTextEdit *m_uriEdit;
    QCheckBox *m_useOriginalCheck;
    QComboBox *m_autoUpdateCombo;
    QLabel *m_hintLabel;
};

class ConfigEditDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ConfigEditDialog(QWidget *parent = nullptr)
        : QDialog(parent)
        , m_editor(new QTextEdit)
    {
        setWindowTitle(tr("Edit current config"));
        setModal(true);
        setMinimumWidth(720);

        QVBoxLayout *layout = new QVBoxLayout(this);
        m_editor->setMinimumHeight(300);
        layout->addWidget(m_editor);

        QHBoxLayout *actions = new QHBoxLayout;
        QPushButton *cancelBtn = new QPushButton(tr("Cancel"));
        QPushButton *okBtn = new QPushButton(tr("Save and Apply"));
        actions->addStretch();
        actions->addWidget(cancelBtn);
        actions->addWidget(okBtn);
        layout->addLayout(actions);

        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    }

    void setContent(const QString &content) { m_editor->setPlainText(content); }
    QString content() const { return m_editor->toPlainText(); }

private:
    QTextEdit *m_editor;
};

// ==================== SubscriptionCard ====================

SubscriptionCard::SubscriptionCard(const SubscriptionInfo &info, bool active, QWidget *parent)
    : QFrame(parent)
    , m_subId(info.id)
{
    setupUI(info, active);
    updateStyle(active);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this, active]() { updateStyle(active); });
}

void SubscriptionCard::setupUI(const SubscriptionInfo &info, bool active)
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
    iconLabel->setFixedSize(32, 32);
    iconLabel->setAlignment(Qt::AlignCenter);

    QLabel *nameLabel = new QLabel(info.name);
    nameLabel->setObjectName("CardName");

    QLabel *typeTag = new QLabel(info.isManual ? tr("Manual Config") : tr("Subscription URL"));
    typeTag->setObjectName("CardTag");

    QLabel *statusTag = new QLabel(active ? tr("Active") : tr("Inactive"));
    statusTag->setObjectName(active ? "CardTagActive" : "CardTag");

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
    QAction *editConfigAction = menu->addAction(tr("Edit Current Config"));
    editConfigAction->setVisible(active);
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
    connect(editConfigAction, &QAction::triggered, [this]() { emit editConfigClicked(m_subId); });
    connect(refreshAction, &QAction::triggered, [this]() { emit refreshClicked(m_subId, false); });
    connect(refreshApplyAction, &QAction::triggered, [this]() { emit refreshClicked(m_subId, true); });
    connect(rollbackAction, &QAction::triggered, [this]() { emit rollbackClicked(m_subId); });
    connect(deleteAction, &QAction::triggered, [this]() { emit deleteClicked(m_subId); });

    headerLayout->addWidget(iconLabel);
    headerLayout->addWidget(nameLabel);
    headerLayout->addWidget(typeTag);
    headerLayout->addWidget(statusTag);
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

    QLabel *timeLabel = new QLabel(tr("Updated: ") + formatTimestamp(info.lastUpdate));
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
                .arg(formatBytes(used))
                .arg(formatBytes(info.subscriptionTotal))
                .arg(formatBytes(remain));
        } else {
            trafficText = tr("Used %1").arg(formatBytes(used));
        }
        QLabel *trafficLabel = new QLabel(tr("Traffic: ") + trafficText);
        trafficLabel->setObjectName("CardInfoText");
        metaLayout->addWidget(trafficLabel);
    }

    if (info.subscriptionExpire > 0) {
        QLabel *expireLabel = new QLabel(tr("Expires: ") + formatExpireTime(info.subscriptionExpire));
        expireLabel->setObjectName("CardInfoText");
        metaLayout->addWidget(expireLabel);
    }

    QPushButton *useBtn = new QPushButton(active ? tr("Use Again") : tr("Use"));
    useBtn->setObjectName("CardActionBtn");
    useBtn->setCursor(Qt::PointingHandCursor);
    useBtn->setFixedHeight(36);
    useBtn->setFixedWidth(110);
    connect(useBtn, &QPushButton::clicked, [this]() { emit useClicked(m_subId); });

    mainLayout->addLayout(headerLayout);
    mainLayout->addLayout(infoLayout);
    if (metaLayout->count() > 0) {
        mainLayout->addLayout(metaLayout);
    }
    mainLayout->addStretch();
    mainLayout->addWidget(useBtn);
}

void SubscriptionCard::updateStyle(bool active)
{
    ThemeManager &tm = ThemeManager::instance();

    setStyleSheet(QString(R"(
        #SubscriptionCard {
            background-color: %1;
            border: 1px solid #353b43;
            border-radius: 10px;
        }
        #CardIcon {
            background-color: %3;
            border-radius: 10px;
            font-size: 16px;
        }
        #CardName {
            font-size: 14px;
            font-weight: bold;
            color: %4;
        }
        #CardTag {
            background-color: %5;
            color: %6;
            padding: 2px 8px;
            border-radius: 10px;
            font-size: 11px;
        }
        #CardTagActive {
            background-color: %3;
            color: white;
            padding: 2px 8px;
            border-radius: 10px;
            font-size: 11px;
        }
        #CardTagSchedule {
            background-color: transparent;
            color: %6;
            padding: 2px 8px;
            border: 1px solid %2;
            border-radius: 10px;
            font-size: 11px;
        }
        #CardMenuBtn {
            background-color: %5;
            color: %4;
            border: 1px solid %2;
            font-size: 16px;
            font-weight: bold;
            padding: 0 8px;
            border-radius: 10px;
        }
        #CardMenuBtn:hover {
            background-color: %3;
            color: white;
        }
        #CardInfoText {
            color: %6;
            font-size: 12px;
        }
        #CardActionBtn {
            background-color: %3;
            color: white;
            border: none;
            border-radius: 10px;
            padding: 8px 16px;
            font-size: 13px;
        }
        #CardActionBtn:hover {
            background-color: %7;
        }
        #SubscriptionMenu {
            background: transparent;
            border: none;
            padding: 6px;
        }
        #SubscriptionMenu::panel {
            background: transparent;
            border: none;
        }
        #SubscriptionMenu::item {
            color: %4;
            padding: 8px 14px;
            border-radius: 10px;
        }
        #SubscriptionMenu::indicator {
            width: 14px;
            height: 14px;
        }
        #SubscriptionMenu::indicator:checked {
            image: url(:/icons/check.svg);
        }
        #SubscriptionMenu::indicator:unchecked {
            image: none;
        }
        #SubscriptionMenu::item:selected {
            background-color: %5;
            color: %4;
        }
        #SubscriptionMenu::separator {
            height: 1px;
            background-color: %2;
            margin: 6px 4px;
        }
    )")
    .arg(active ? tm.getColorString("bg-tertiary") : tm.getColorString("bg-secondary"))
    .arg(active ? tm.getColorString("primary") : tm.getColorString("border"))
    .arg(tm.getColorString("primary"))
    .arg(tm.getColorString("text-primary"))
    .arg(tm.getColorString("bg-tertiary"))
    .arg(tm.getColorString("text-secondary"))
    .arg(tm.getColorString("primary") + "cc")
    );
}
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
    m_addBtn->setObjectName("PrimaryActionBtn");
    m_addBtn->setCursor(Qt::PointingHandCursor);
    m_addBtn->setMinimumSize(110, 36);

    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    headerLayout->addWidget(m_addBtn);

    mainLayout->addLayout(headerLayout);

    QHBoxLayout *toolbarLayout = new QHBoxLayout;

    m_updateAllBtn = new QPushButton(tr("Update All"));
    m_updateAllBtn->setCursor(Qt::PointingHandCursor);
    m_updateAllBtn->setMinimumHeight(32);

    toolbarLayout->addWidget(m_updateAllBtn);
    toolbarLayout->addStretch();

    mainLayout->addLayout(toolbarLayout);

    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_cardsContainer = new QWidget;
    m_cardsLayout = new QVBoxLayout(m_cardsContainer);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(12);
    m_cardsLayout->addStretch();

    m_scrollArea->setWidget(m_cardsContainer);

    mainLayout->addWidget(m_scrollArea, 1);

    connect(m_addBtn, &QPushButton::clicked, this, &SubscriptionView::onAddClicked);
    connect(m_updateAllBtn, &QPushButton::clicked, this, &SubscriptionView::onUpdateAllClicked);

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
    )")
    .arg(tm.getColorString("text-primary"))
    .arg(tm.getColorString("text-secondary")));

    m_addBtn->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: white; border-radius: 10px; padding: 8px 18px; border: none; }"
        "QPushButton:hover { background-color: %2; }"
    ).arg(tm.getColorString("primary")).arg(tm.getColorString("primary") + "cc"));

    m_updateAllBtn->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: %2; border-radius: 10px; padding: 6px 14px; border: none; }"
        "QPushButton:hover { background-color: %3; }"
    ).arg(tm.getColorString("bg-tertiary"))
     .arg(tm.getColorString("text-primary"))
     .arg(tm.getColorString("primary") + "40"));

    m_scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    m_cardsContainer->setStyleSheet("background: transparent;");
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

void SubscriptionView::onUpdateAllClicked()
{
    m_subscriptionService->updateAllSubscriptions(false);
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

    connect(card, &SubscriptionCard::useClicked, [this](const QString &id) {
        m_subscriptionService->setActiveSubscription(id, true);
    });

    connect(card, &SubscriptionCard::editClicked, [this](const QString &id) {
        const QList<SubscriptionInfo> subs = m_subscriptionService->getSubscriptions();
        SubscriptionInfo target;
        bool found = false;
        for (const auto &sub : subs) {
            if (sub.id == id) {
                target = sub;
                found = true;
                break;
            }
        }
        if (!found) return;

        SubscriptionFormDialog dialog(this);
        dialog.setEditData(target);
        if (dialog.exec() != QDialog::Accepted) return;

        QString error;
        if (!dialog.validateInput(&error)) {
            QMessageBox::warning(this, tr("Notice"), error);
            return;
        }

        bool isManual = dialog.isManual();
        QString content = dialog.isUriList() ? dialog.uriContent() : dialog.manualContent();
        m_subscriptionService->updateSubscriptionMeta(
            id,
            dialog.name(),
            dialog.url(),
            isManual,
            content,
            dialog.useOriginalConfig(),
            dialog.autoUpdateIntervalMinutes()
        );
    });

    connect(card, &SubscriptionCard::editConfigClicked, [this](const QString &id) {
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
    });

    connect(card, &SubscriptionCard::refreshClicked, [this](const QString &id, bool applyRuntime) {
        m_subscriptionService->refreshSubscription(id, applyRuntime);
    });

    connect(card, &SubscriptionCard::rollbackClicked, [this](const QString &id) {
        const QList<SubscriptionInfo> subs = m_subscriptionService->getSubscriptions();
        for (const auto &sub : subs) {
            if (sub.id == id) {
                if (!m_subscriptionService->rollbackSubscriptionConfig(sub.configPath)) {
                    QMessageBox::warning(this, tr("Notice"), tr("No config available to roll back"));
                    return;
                }
                if (m_subscriptionService->getActiveIndex() >= 0) {
                    m_subscriptionService->setActiveSubscription(id, true);
                }
                return;
            }
        }
    });

    connect(card, &SubscriptionCard::deleteClicked, [this](const QString &id) {
        if (QMessageBox::question(this, tr("Confirm"), tr("Are you sure you want to delete this subscription?")) == QMessageBox::Yes) {
            m_subscriptionService->removeSubscription(id);
        }
    });

    connect(card, &SubscriptionCard::copyLinkClicked, [this](const QString &id) {
        const QList<SubscriptionInfo> subs = m_subscriptionService->getSubscriptions();
        for (const auto &sub : subs) {
            if (sub.id == id) {
                QApplication::clipboard()->setText(sub.url);
                break;
            }
        }
    });

    return card;
}

void SubscriptionView::refreshList()
{
    while (m_cardsLayout->count() > 1) {
        QLayoutItem *item = m_cardsLayout->takeAt(0);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    const QList<SubscriptionInfo> subs = m_subscriptionService->getSubscriptions();
    const int activeIndex = m_subscriptionService->getActiveIndex();
    for (int i = 0; i < subs.count(); ++i) {
        SubscriptionCard *card = createSubscriptionCard(subs[i], i == activeIndex);
        m_cardsLayout->insertWidget(m_cardsLayout->count() - 1, card);
    }
}

#include "SubscriptionView.moc"
