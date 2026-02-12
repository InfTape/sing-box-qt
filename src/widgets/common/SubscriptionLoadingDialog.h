#ifndef SUBSCRIPTIONLOADINGDIALOG_H
#define SUBSCRIPTIONLOADINGDIALOG_H

#include <QDialog>

class QTimer;

class SubscriptionLoadingDialog : public QDialog {
  Q_OBJECT
 public:
  explicit SubscriptionLoadingDialog(QWidget* parent = nullptr);

  void showLoading(QWidget* anchor = nullptr);
  void showSuccessAndAutoClose(int msecs = 700);
  void closePopup();

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  enum class State { Loading, Success };

  void centerToAnchor(QWidget* anchor);

  State   m_state = State::Loading;
  QTimer* m_spinTimer;
  QTimer* m_closeTimer;
  int     m_angle = 0;
};

#endif  // SUBSCRIPTIONLOADINGDIALOG_H
