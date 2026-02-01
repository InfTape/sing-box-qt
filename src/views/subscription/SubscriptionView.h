#ifndef SUBSCRIPTIONVIEW_H
#define SUBSCRIPTIONVIEW_H

#include <QWidget>
#include <QString>
#include <QPushButton>
#include <QGridLayout>
#include <QList>
#include <QScrollArea>
#include <QLabel>
#include <QResizeEvent>
#include <QTimer>

class QScrollArea;
class SubscriptionCard;
class NodeEditDialog;
class SubscriptionService;
struct SubscriptionInfo;

class SubscriptionView : public QWidget
{
    Q_OBJECT

public:
    explicit SubscriptionView(SubscriptionService *service, QWidget *parent = nullptr);
    SubscriptionService* getService() const;

private slots:
    void onAddClicked();
    void onAutoUpdateTimer();
    void updateStyle();

private:
    void setupUI();
    void refreshList();
    void layoutCards();
    SubscriptionCard* createSubscriptionCard(const SubscriptionInfo &info, bool active);
    void wireCardSignals(SubscriptionCard *card);
    void handleUseSubscription(const QString &id);
    void handleEditSubscription(const QString &id);
    void handleEditConfig(const QString &id);
    void handleRefreshSubscription(const QString &id, bool applyRuntime);
    void handleRollbackSubscription(const QString &id);
    void handleDeleteSubscription(const QString &id);
    void handleCopyLink(const QString &id);
    
    // New slots
    void onAddNodeClicked();
    void openSubscriptionDialog();

    bool getSubscriptionById(const QString &id, SubscriptionInfo *out) const;

protected:
    void resizeEvent(QResizeEvent *event) override;
    
    QPushButton *m_addBtn;
    QScrollArea *m_scrollArea;
    QWidget *m_cardsContainer;
    QGridLayout *m_cardsLayout;
    QList<SubscriptionCard*> m_cards;
    int m_columnCount = 0;
    SubscriptionService *m_subscriptionService;
    QTimer *m_autoUpdateTimer;
};

#endif // SUBSCRIPTIONVIEW_H
