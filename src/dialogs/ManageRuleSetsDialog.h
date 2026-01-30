#ifndef MANAGERULESETSDIALOG_H
#define MANAGERULESETSDIALOG_H

#include <QDialog>
#include <QStringList>
#include "services/RuleConfigService.h"

class QListWidget;

class ManageRuleSetsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ManageRuleSetsDialog(QWidget *parent = nullptr);

signals:
    void ruleSetsChanged();

private slots:
    void onSetAdd();
    void onSetRename();
    void onSetDelete();
    void onRuleAdd();
    void onRuleDelete();
    void onSetChanged();
    void onSetContextMenu(const QPoint &pos);
    void onRuleContextMenu(const QPoint &pos);

private:
    void reload();
    QString selectedName() const;
    void reloadRules();
    bool confirmDelete(const QString &name);
    void addRuleToSet(const QString &setName, const RuleConfigService::RuleEditData &data);
    void updateMenus();

    QListWidget *m_list;
    QListWidget *m_ruleList;
};

#endif // MANAGERULESETSDIALOG_H
