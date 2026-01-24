#include "dialogs/SubscriptionFormDialog.h"
#include "network/SubscriptionService.h"
#include "widgets/MenuComboBox.h"
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

namespace {
bool isJsonText(const QString &text)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &err);
    return err.error == QJsonParseError::NoError && doc.isObject();
}
} // namespace

SubscriptionFormDialog::SubscriptionFormDialog(QWidget *parent)
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
}
