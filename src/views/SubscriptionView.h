#ifndef SUBSCRIPTIONVIEW_H
#define SUBSCRIPTIONVIEW_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QFrame>
#include <QTimer>

class SubscriptionService;
struct SubscriptionInfo;

class SubscriptionCard : public QFrame
{
    Q_OBJECT
public:
    explicit SubscriptionCard(const SubscriptionInfo &info, bool active, QWidget *parent = nullptr);
    QString subscriptionId() const { return m_subId; }

signals:
    void useClicked(const QString &id);
    void editClicked(const QString &id);
    void editConfigClicked(const QString &id);
    void refreshClicked(const QString &id, bool applyRuntime);
    void rollbackClicked(const QString &id);
    void deleteClicked(const QString &id);
    void copyLinkClicked(const QString &id);

private:
    void setupUI(const SubscriptionInfo &info, bool active);
    void updateStyle(bool active);
    QString m_subId;
};

class SubscriptionView : public QWidget
{
    Q_OBJECT

public:
    explicit SubscriptionView(QWidget *parent = nullptr);
    SubscriptionService* getService() const;

private slots:
    void onAddClicked();
    void onUpdateAllClicked();
    void onAutoUpdateTimer();
    void updateStyle();

private:
    void setupUI();
    void refreshList();
    SubscriptionCard* createSubscriptionCard(const SubscriptionInfo &info, bool active);
    
    QPushButton *m_addBtn;
    QPushButton *m_updateAllBtn;
    QScrollArea *m_scrollArea;
    QWidget *m_cardsContainer;
    QVBoxLayout *m_cardsLayout;
    SubscriptionService *m_subscriptionService;
    QTimer *m_autoUpdateTimer;
};

#endif // SUBSCRIPTIONVIEW_H
