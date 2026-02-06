#ifndef SUBSCRIPTIONVIEW_H
#define SUBSCRIPTIONVIEW_H
#include <QGridLayout>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QString>
#include <QTimer>
#include <QWidget>
class QScrollArea;
class QEvent;
class QShowEvent;
class SubscriptionCard;
class NodeEditDialog;
class SubscriptionService;
class ThemeService;
struct SubscriptionInfo;

class SubscriptionView : public QWidget {
  Q_OBJECT
 public:
  explicit SubscriptionView(SubscriptionService* service, ThemeService* themeService, QWidget* parent = nullptr);
  SubscriptionService* getService() const;
 private slots:
  void onAddClicked();
  void onAutoUpdateTimer();
  void updateStyle();
  void handleSubscriptionUpdated(const QString& id);
  void handleActiveSubscriptionChanged(const QString& id, const QString& configPath);

 private:
  void              setupUI();
  void              refreshList();
  void              layoutCards();
  SubscriptionCard* createSubscriptionCard(const SubscriptionInfo& info, bool active);
  void              wireCardSignals(SubscriptionCard* card);
  void              handleUseSubscription(const QString& id);
  void              handleEditSubscription(const QString& id);
  void              handleEditConfig(const QString& id);
  void              handleRefreshSubscription(const QString& id, bool applyRuntime);
  void              handleRollbackSubscription(const QString& id);
  void              handleDeleteSubscription(const QString& id);
  void              handleCopyLink(const QString& id);
  // New slots
  void              onAddNodeClicked();
  void              openSubscriptionDialog();
  SubscriptionCard* findCardById(const QString& id) const;
  void              updateActiveCards(const QString& activeId);
  bool              getSubscriptionById(const QString& id, SubscriptionInfo* out) const;

 protected:
  void                          resizeEvent(QResizeEvent* event) override;
  bool                          eventFilter(QObject* watched, QEvent* event) override;
  void                          showEvent(QShowEvent* event) override;
  QPushButton*                  m_addBtn;
  QScrollArea*                  m_scrollArea;
  QWidget*                      m_cardsContainer;
  QGridLayout*                  m_cardsLayout;
  QList<SubscriptionCard*>      m_cards;
  int                           m_columnCount       = 0;
  bool                          m_skipNextAnimation = false;
  SubscriptionService*          m_subscriptionService;
  class SubscriptionController* m_subscriptionController;
  QTimer*                       m_autoUpdateTimer;
  ThemeService*                 m_themeService = nullptr;
};
#endif  // SUBSCRIPTIONVIEW_H
