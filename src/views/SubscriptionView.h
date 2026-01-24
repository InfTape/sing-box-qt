#ifndef SUBSCRIPTIONVIEW_H
#define SUBSCRIPTIONVIEW_H

#include <QWidget>
#include <QString>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QTimer>

class SubscriptionCard;
class SubscriptionService;
struct SubscriptionInfo;

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
    void wireCardSignals(SubscriptionCard *card);
    void handleUseSubscription(const QString &id);
    void handleEditSubscription(const QString &id);
    void handleEditConfig(const QString &id);
    void handleRefreshSubscription(const QString &id, bool applyRuntime);
    void handleRollbackSubscription(const QString &id);
    void handleDeleteSubscription(const QString &id);
    void handleCopyLink(const QString &id);
    bool getSubscriptionById(const QString &id, SubscriptionInfo *out) const;
    
    QPushButton *m_addBtn;
    QPushButton *m_updateAllBtn;
    QScrollArea *m_scrollArea;
    QWidget *m_cardsContainer;
    QVBoxLayout *m_cardsLayout;
    SubscriptionService *m_subscriptionService;
    QTimer *m_autoUpdateTimer;
};

#endif // SUBSCRIPTIONVIEW_H
