#include "dialogs/rules/RuleEditorDialog.h"
#include "utils/rule/RuleUtils.h"
#include "widgets/common/MenuComboBox.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

RuleEditorDialog::RuleEditorDialog(Mode mode, QWidget *parent)
    : QDialog(parent)
    , m_mode(mode)
    , m_fields(RuleConfigService::fieldInfos())
    , m_typeCombo(new MenuComboBox(this))
    , m_valueEdit(new QPlainTextEdit(this))
    , m_outboundCombo(new MenuComboBox(this))
    , m_ruleSetEdit(new QLineEdit(this))
    , m_hintLabel(new QLabel(this))
{
    setModal(true);
    setWindowTitle(m_mode == Mode::Add ? tr("Add Rule") : tr("Edit Match Type"));

    QVBoxLayout *layout = new QVBoxLayout(this);
    QFormLayout *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setFormAlignment(Qt::AlignLeft);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);

    for (const auto &field : m_fields) {
        m_typeCombo->addItem(field.label, field.key);
    }

    if (!m_fields.isEmpty()) {
        updatePlaceholder(0);
    }

    m_valueEdit->setFixedHeight(80);

    form->addRow(tr("Match Type:"), m_typeCombo);
    form->addRow(tr("Match Value:"), m_valueEdit);
    form->addRow(tr("Outbound:"), m_outboundCombo);
    m_ruleSetEdit->setPlaceholderText("default");
    form->addRow(tr("Rule Set:"), m_ruleSetEdit);

    m_hintLabel->setWordWrap(true);
    m_hintLabel->setObjectName("RuleHint");
    m_hintLabel->setText(tr("Note: rules are written to route.rules (1.11+ format). Restart kernel or app to apply."));
    m_hintLabel->setVisible(m_mode == Mode::Add);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("OK"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));

    layout->addLayout(form);
    layout->addWidget(m_hintLabel);
    layout->addWidget(buttons);

    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RuleEditorDialog::updatePlaceholder);
    connect(buttons, &QDialogButtonBox::accepted, this, &RuleEditorDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &RuleEditorDialog::reject);
}

void RuleEditorDialog::setOutboundTags(const QStringList &tags)
{
    m_outboundCombo->clear();
    m_outboundCombo->addItems(tags);
}

void RuleEditorDialog::setRuleSetName(const QString &name)
{
    m_ruleSetEdit->setText(name.isEmpty() ? QStringLiteral("default") : name);
}

bool RuleEditorDialog::setEditRule(const RuleItem &rule, QString *error)
{
    QString key;
    QStringList values;
    if (!RuleConfigService::parseRulePayload(rule.payload, &key, &values, error)) {
        return false;
    }

    int index = -1;
    for (int i = 0; i < m_fields.size(); ++i) {
        if (m_fields[i].key == key) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        if (error) *error = tr("Failed to parse current rule content.");
        return false;
    }

    m_typeCombo->setCurrentIndex(index);
    m_valueEdit->setPlainText(values.join(","));

    const QString outbound = RuleUtils::normalizeProxyValue(rule.proxy);
    const int outboundIndex = m_outboundCombo->findText(outbound);
    if (outboundIndex >= 0) {
        m_outboundCombo->setCurrentIndex(outboundIndex);
    }
    return true;
}

RuleConfigService::RuleEditData RuleEditorDialog::editData() const
{
    return m_cachedData;
}

void RuleEditorDialog::accept()
{
    RuleConfigService::RuleEditData data;
    QString error;
    if (!buildEditData(&data, &error)) {
        const QString title = m_mode == Mode::Add ? tr("Add Rule") : tr("Edit Match Type");
        QMessageBox::warning(this, title, error);
        return;
    }
    m_cachedData = data;
    QDialog::accept();
}

void RuleEditorDialog::updatePlaceholder(int index)
{
    if (index < 0 || index >= m_fields.size()) return;
    m_valueEdit->setPlaceholderText(m_fields[index].placeholder + tr(" (separate by commas or new lines)"));
}

bool RuleEditorDialog::buildEditData(RuleConfigService::RuleEditData *out, QString *error) const
{
    if (!out) return false;
    const int fieldIndex = m_typeCombo->currentIndex();
    if (fieldIndex < 0 || fieldIndex >= m_fields.size()) {
        if (error) *error = tr("Match type cannot be empty.");
        return false;
    }

    const RuleConfigService::RuleFieldInfo field = m_fields[fieldIndex];

    const QString rawText = m_valueEdit->toPlainText().trimmed();
    if (rawText.isEmpty()) {
        if (error) *error = tr("Match value cannot be empty.");
        return false;
    }

    QStringList values = rawText.split(QRegularExpression("[,\\n]"), Qt::SkipEmptyParts);
    for (QString &v : values) v = v.trimmed();
    values.removeAll(QString());
    if (values.isEmpty()) {
        if (error) *error = tr("Match value cannot be empty.");
        return false;
    }

    if (field.key == "ip_is_private") {
        if (values.size() != 1) {
            if (error) *error = tr("ip_is_private allows only one value (true/false).");
            return false;
        }
        const QString raw = values.first().toLower();
        if (raw != "true" && raw != "false") {
            if (error) *error = tr("ip_is_private must be true or false.");
            return false;
        }
        values = {raw};
    } else if (field.numeric) {
        for (const auto &v : values) {
            bool ok = false;
            v.toInt(&ok);
            if (!ok) {
                if (error) *error = tr("Port must be numeric: %1").arg(v);
                return false;
            }
        }
    }

    const QString outboundTag = m_outboundCombo->currentText().trimmed();
    if (outboundTag.isEmpty()) {
        if (error) *error = tr("Outbound cannot be empty.");
        return false;
    }

    out->field = field;
    out->values = values;
    out->outboundTag = outboundTag;
    const QString rs = m_ruleSetEdit->text().trimmed();
    out->ruleSet = rs.isEmpty() ? QStringLiteral("default") : rs;
    return true;
}
