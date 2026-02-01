#ifndef MANAGERULESETSDIALOG_H
#define MANAGERULESETSDIALOG_H

#include <QDialog>
#include <QStringList>
#include "services/rules/RuleConfigService.h"

class QListWidget;
class ThemeService;
class ConfigRepository;

class ManageRuleSetsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ManageRuleSetsDialog(ConfigRepository *configRepo, ThemeService *themeService, QWidget *parent = nullptr);

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
    ConfigRepository *m_configRepo = nullptr;
    ThemeService *m_themeService = nullptr;
};

#endif // MANAGERULESETSDIALOG_H
