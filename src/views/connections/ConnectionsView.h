#ifndef CONNECTIONSVIEW_H
#define CONNECTIONSVIEW_H
#include <QHash>
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
  void          sortConnections();
  QTableWidget* m_tableWidget;

  int           m_sortColumn = -1;
  Qt::SortOrder m_sortOrder  = Qt::AscendingOrder;
  // Start time in milliseconds and stable arrival sequence, keyed by ID.
  QHash<QString, QPair<qint64, quint64>> m_connectionOrder;
  quint64 m_nextConnectionOrder = 0;

  QPushButton*  m_closeAllBtn;
  ProxyService* m_proxyService;
  bool          m_autoRefreshEnabled = false;
  ThemeService* m_themeService       = nullptr;
};
#endif  // CONNECTIONSVIEW_H
