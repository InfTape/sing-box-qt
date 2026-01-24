#ifndef RULEEDITORDIALOG_H
#define RULEEDITORDIALOG_H

#include <QDialog>
#include "services/RuleConfigService.h"

class QLabel;
class QPlainTextEdit;
class MenuComboBox;

class RuleEditorDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode {
        Add,
        Edit
    };

    explicit RuleEditorDialog(Mode mode, QWidget *parent = nullptr);

    void setOutboundTags(const QStringList &tags);
    bool setEditRule(const RuleItem &rule, QString *error);
    RuleConfigService::RuleEditData editData() const;

protected:
    void accept() override;

private:
    void updatePlaceholder(int index);
    bool buildEditData(RuleConfigService::RuleEditData *out, QString *error) const;

    Mode m_mode;
    QList<RuleConfigService::RuleFieldInfo> m_fields;
    MenuComboBox *m_typeCombo;
    QPlainTextEdit *m_valueEdit;
    MenuComboBox *m_outboundCombo;
    QLabel *m_hintLabel;
    RuleConfigService::RuleEditData m_cachedData;
};

#endif // RULEEDITORDIALOG_H
