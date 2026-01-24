#ifndef SUBSCRIPTIONFORMDIALOG_H
#define SUBSCRIPTIONFORMDIALOG_H

#include <QDialog>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QTabWidget;
class QTextEdit;
struct SubscriptionInfo;

class SubscriptionFormDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SubscriptionFormDialog(QWidget *parent = nullptr);

    void setEditData(const SubscriptionInfo &info);

    QString name() const;
    QString url() const;
    QString manualContent() const;
    QString uriContent() const;
    bool isManual() const;
    bool isUriList() const;
    bool useOriginalConfig() const;
    int autoUpdateIntervalMinutes() const;

    bool validateInput(QString *error) const;

private:
    QWidget* createTextTab(QTextEdit *edit, const QString &placeholder);
    int indexForInterval(int minutes) const;
    void updateState();

    QLineEdit *m_nameEdit;
    QTabWidget *m_tabs;
    QTextEdit *m_urlEdit;
    QTextEdit *m_manualEdit;
    QTextEdit *m_uriEdit;
    QCheckBox *m_useOriginalCheck;
    QComboBox *m_autoUpdateCombo;
    QLabel *m_hintLabel;
};

#endif // SUBSCRIPTIONFORMDIALOG_H
