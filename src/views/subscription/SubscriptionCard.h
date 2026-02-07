#ifndef SUBSCRIPTIONCARD_H
#define SUBSCRIPTIONCARD_H
#include <QFrame>
#include <QString>
class QAction;
class QEvent;
class QLabel;
class QPushButton;
class QResizeEvent;
class ThemeService;
struct SubscriptionInfo;

class SubscriptionCard : public QFrame {
  Q_OBJECT
 public:
  explicit SubscriptionCard(const SubscriptionInfo& info,
                            bool                    active,
                            ThemeService*           themeService,
                            QWidget*                parent = nullptr);

  QString subscriptionId() const { return m_subId; }

  void setActive(bool active);
  void updateInfo(const SubscriptionInfo& info, bool active);
 signals:
  void useClicked(const QString& id);
  void editClicked(const QString& id);
  void editConfigClicked(const QString& id);
  void refreshClicked(const QString& id, bool applyRuntime);
  void rollbackClicked(const QString& id);
  void deleteClicked(const QString& id);
  void copyLinkClicked(const QString& id);

 private:
  void          setupUI(const SubscriptionInfo& info);
  void          applyActiveState();
  void          updateStyle();
  void          updateUrlLabelText();
  void          resizeEvent(QResizeEvent* event) override;
  bool          eventFilter(QObject* watched, QEvent* event) override;
  QString       m_subId;
  bool          m_active           = false;
  QLabel*       m_nameLabel        = nullptr;
  QLabel*       m_typeTag          = nullptr;
  QLabel*       m_statusTag        = nullptr;
  QLabel*       m_scheduleTag      = nullptr;
  QLabel*       m_urlLabel         = nullptr;
  QLabel*       m_timeLabel        = nullptr;
  QLabel*       m_trafficLabel     = nullptr;
  QLabel*       m_expireLabel      = nullptr;
  QPushButton*  m_useBtn           = nullptr;
  QAction*      m_editConfigAction = nullptr;
  ThemeService* m_themeService     = nullptr;
  QString       m_urlRawText;
};
#endif  // SUBSCRIPTIONCARD_H
