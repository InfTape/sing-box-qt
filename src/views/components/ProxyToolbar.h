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
  void setTesting(bool testing);
  void setTestAllText(const QString& text);
  void setProgress(int progress);
  void showProgress(bool visible);
 signals:
  void searchTextChanged(const QString& text);
  void testAllClicked();
  void refreshClicked();

 private:
  QLineEdit*    m_searchEdit  = nullptr;
  QPushButton*  m_testAllBtn  = nullptr;
  QPushButton*  m_refreshBtn  = nullptr;
  QProgressBar* m_progressBar = nullptr;
};
#endif  // PROXYTOOLBAR_H
