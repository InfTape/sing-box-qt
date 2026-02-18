#ifndef CONNECTIONSVIEW_H
#define CONNECTIONSVIEW_H
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>
class ProxyService;
class ThemeService;

class ConnectionsView : public QWidget {
  Q_OBJECT
 public:
  explicit ConnectionsView(ThemeService* themeService,
                           QWidget*      parent = nullptr);
  void setProxyService(ProxyService* service);
  void setAutoRefreshEnabled(bool enabled);
 private slots:
  void onCloseAll();
  void updateStyle();

 private:
  void          setupUI();
  QTableWidget* m_tableWidget;

  QPushButton*  m_closeAllBtn;
  ProxyService* m_proxyService;
  bool          m_autoRefreshEnabled = false;
  ThemeService* m_themeService       = nullptr;
};
#endif  // CONNECTIONSVIEW_H
