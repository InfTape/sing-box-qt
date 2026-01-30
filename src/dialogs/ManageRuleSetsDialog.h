#ifndef MANAGERULESETSDIALOG_H
#define MANAGERULESETSDIALOG_H

#include <QDialog>
#include <QStringList>

class QListWidget;
class QPushButton;

class ManageRuleSetsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ManageRuleSetsDialog(QWidget *parent = nullptr);

signals:
    void ruleSetsChanged();

private slots:
    void onAdd();
    void onRename();
    void onDelete();

private:
    void reload();
    QString selectedName() const;
    bool confirmDelete(const QString &name);

    QListWidget *m_list;
    QPushButton *m_addBtn;
    QPushButton *m_renameBtn;
    QPushButton *m_deleteBtn;
};

#endif // MANAGERULESETSDIALOG_H
