#ifndef PROXYTOOLBAR_H
#define PROXYTOOLBAR_H
#include <QFrame>
class QLineEdit;
class QPushButton;
class QProgressBar;

class ProxyToolbar : public QFrame {
  Q_OBJECT
 public:
  explicit ProxyToolbar(QWidget* parent = nullptr);

  void setProgress(int progress);
  void showProgress(bool visible);
 signals:
  void searchTextChanged(const QString& text);
  void addGroupClicked();


 private:
  QLineEdit*    m_searchEdit  = nullptr;
  QPushButton*  m_addGroupBtn = nullptr;

  QProgressBar* m_progressBar = nullptr;
};
#endif  // PROXYTOOLBAR_H
