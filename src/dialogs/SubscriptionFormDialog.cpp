#include "dialogs/SubscriptionFormDialog.h"
#include "network/SubscriptionService.h"
#include "widgets/MenuComboBox.h"
#include "widgets/RoundedMenu.h"
#include "utils/ThemeManager.h"
#include "services/SharedRulesStore.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QPushButton>
#include <QToolButton>
#include <QAction>

namespace {
bool isJsonText(const QString &text)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &err);
    return err.error == QJsonParseError::NoError && doc.isObject();
}
} // namespace

// ===== MultiSelectMenuBox =====
MultiSelectMenuBox::MultiSelectMenuBox(QWidget *parent)
    : QWidget(parent)
{
    m_button = new QToolButton(this);
    m_button->setText(tr("default"));
    m_button->setPopupMode(QToolButton::InstantPopup);
    m_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_button->setAutoRaise(true);
    m_menu = new RoundedMenu(this);
    // Use tray menu styling so the checklist matches the tray menu look
    m_menu->setObjectName("TrayMenu");

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_button);

    connect(m_button, &QToolButton::clicked, this, [this]() {
        rebuildMenu();
        m_menu->popup(m_button->mapToGlobal(QPoint(0, m_button->height())));
    });

    ThemeManager &tm = ThemeManager::instance();
    connect(&tm, &ThemeManager::themeChanged, this, [this]() {
        m_menu->setThemeColors(ThemeManager::instance().getColor("bg-secondary"),
                               ThemeManager::instance().getColor("primary"));
    });
    m_menu->setThemeColors(tm.getColor("bg-secondary"), tm.getColor("primary"));
}

void MultiSelectMenuBox::setOptions(const QStringList &options)
{
    m_options = options;
    m_options.removeDuplicates();
    m_options.removeAll(QString());
    m_options.sort();
    if (!m_options.contains("default")) m_options.prepend("default");
    rebuildMenu();
}

void MultiSelectMenuBox::setSelected(const QStringList &selected)
{
    m_selected = selected;
    if (m_selected.isEmpty()) m_selected << "default";
    m_selected.removeDuplicates();
    updateButtonText();
}

QStringList MultiSelectMenuBox::selected() const
{
    return m_selected;
}

void MultiSelectMenuBox::rebuildMenu()
{
    m_menu->clear();
    for (const QString &name : m_options) {
        QAction *act = m_menu->addAction(name);
        act->setCheckable(true);
        act->setChecked(m_selected.contains(name));
        connect(act, &QAction::triggered, this, [this, name, act]() {
            if (act->isChecked()) {
                if (!m_selected.contains(name)) m_selected << name;
            } else {
                m_selected.removeAll(name);
            }
            if (m_selected.isEmpty()) m_selected << "default";
            updateButtonText();
            emit selectionChanged(m_selected);
        });
    }
}

void MultiSelectMenuBox::updateButtonText()
{
    QString text = m_selected.join(", ");
    if (text.isEmpty()) text = tr("default");
    m_button->setText(text);
}

SubscriptionFormDialog::SubscriptionFormDialog(QWidget *parent)
    : QDialog(parent)
    , m_nameEdit(new QLineEdit)
    , m_tabs(new QTabWidget)
    , m_urlEdit(new QTextEdit)
    , m_manualEdit(new QTextEdit)
    , m_uriEdit(new QTextEdit)
    , m_useOriginalCheck(new QCheckBox(tr("Use original config")))
    , m_sharedRulesCheck(new QCheckBox(tr("Enable shared rule set")))
    , m_ruleSetsBox(new MultiSelectMenuBox)
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
    m_hintLabel->setObjectName("SubscriptionHint");
    m_hintLabel->setText(tr("Advanced templates are disabled when using the original config"));
    m_hintLabel->setVisible(false);

    QStringList initialSets = SharedRulesStore::listRuleSets();
    m_ruleSetsBox->setOptions(initialSets);
    m_ruleSetsBox->setSelected(QStringList() << "default");

    layout->addLayout(formLayout);
    layout->addWidget(m_tabs);
    layout->addWidget(m_useOriginalCheck);
    m_sharedRulesCheck->setChecked(true);
    layout->addWidget(m_sharedRulesCheck);

    QFormLayout *ruleSetForm = new QFormLayout;
    ruleSetForm->setLabelAlignment(Qt::AlignRight);
    ruleSetForm->addRow(tr("Rule sets"), m_ruleSetsBox);
    layout->addLayout(ruleSetForm);
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
    connect(m_sharedRulesCheck, &QCheckBox::toggled, this, &SubscriptionFormDialog::updateState);
    connect(m_ruleSetsBox, &MultiSelectMenuBox::selectionChanged, this, &SubscriptionFormDialog::updateState);

    updateState();
}

void SubscriptionFormDialog::setEditData(const SubscriptionInfo &info)
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
    m_sharedRulesCheck->setChecked(info.enableSharedRules);
    QStringList options = SharedRulesStore::listRuleSets();
    for (const auto &name : info.ruleSets) if (!options.contains(name)) options << name;
    m_ruleSetsBox->setOptions(options);
    m_ruleSetsBox->setSelected(info.ruleSets.isEmpty() ? QStringList{"default"} : info.ruleSets);
    m_autoUpdateCombo->setCurrentIndex(indexForInterval(info.autoUpdateIntervalMinutes));
    updateState();
}

QString SubscriptionFormDialog::name() const { return m_nameEdit->text().trimmed(); }
QString SubscriptionFormDialog::url() const { return m_urlEdit->toPlainText().trimmed(); }
QString SubscriptionFormDialog::manualContent() const { return m_manualEdit->toPlainText().trimmed(); }
QString SubscriptionFormDialog::uriContent() const { return m_uriEdit->toPlainText().trimmed(); }
bool SubscriptionFormDialog::isManual() const { return m_tabs->currentIndex() != 0; }
bool SubscriptionFormDialog::isUriList() const { return m_tabs->currentIndex() == 2; }
bool SubscriptionFormDialog::useOriginalConfig() const { return m_useOriginalCheck->isChecked(); }
bool SubscriptionFormDialog::sharedRulesEnabled() const { return m_sharedRulesCheck->isChecked(); }
QStringList SubscriptionFormDialog::ruleSets() const
{
    QStringList list = m_ruleSetsBox->selected();
    if (list.isEmpty()) list << "default";
    list.removeDuplicates();
    return list;
}
int SubscriptionFormDialog::autoUpdateIntervalMinutes() const { return m_autoUpdateCombo->currentData().toInt(); }

bool SubscriptionFormDialog::validateInput(QString *error) const
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

QWidget* SubscriptionFormDialog::createTextTab(QTextEdit *edit, const QString &placeholder)
{
    QWidget *widget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(widget);
    edit->setPlaceholderText(placeholder + "...");
    edit->setMinimumHeight(110);
    layout->addWidget(edit);
    return widget;
}

int SubscriptionFormDialog::indexForInterval(int minutes) const
{
    for (int i = 0; i < m_autoUpdateCombo->count(); ++i) {
        if (m_autoUpdateCombo->itemData(i).toInt() == minutes) {
            return i;
        }
    }
    return 0;
}

void SubscriptionFormDialog::updateState()
{
    const bool urlTab = m_tabs->currentIndex() == 0;
    m_autoUpdateCombo->setEnabled(urlTab);
    if (!urlTab) {
        m_autoUpdateCombo->setCurrentIndex(0);
    }
    const bool showHint = m_useOriginalCheck->isChecked();
    m_hintLabel->setVisible(showHint);
    m_ruleSetsBox->setEnabled(m_sharedRulesCheck->isChecked());
}
