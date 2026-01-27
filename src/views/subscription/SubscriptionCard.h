#ifndef SUBSCRIPTIONCARD_H
#define SUBSCRIPTIONCARD_H

#include <QFrame>
#include <QString>

class QAction;
class QLabel;
class QPushButton;
struct SubscriptionInfo;

class SubscriptionCard : public QFrame
{
    Q_OBJECT

public:
    explicit SubscriptionCard(const SubscriptionInfo &info, bool active, QWidget *parent = nullptr);
    QString subscriptionId() const { return m_subId; }
    void setActive(bool active);

signals:
    void useClicked(const QString &id);
    void editClicked(const QString &id);
    void editConfigClicked(const QString &id);
    void refreshClicked(const QString &id, bool applyRuntime);
    void rollbackClicked(const QString &id);
    void deleteClicked(const QString &id);
    void copyLinkClicked(const QString &id);

private:
    void setupUI(const SubscriptionInfo &info);
    void applyActiveState();
    void updateStyle();
    void applyUseButtonStyle();

    QString m_subId;
    bool m_active = false;
    QLabel *m_statusTag = nullptr;
    QPushButton *m_useBtn = nullptr;
    QAction *m_editConfigAction = nullptr;
};

#endif // SUBSCRIPTIONCARD_H
