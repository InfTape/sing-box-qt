#ifndef DATAUSAGECARD_H
#define DATAUSAGECARD_H
#include <QFrame>
#include <QJsonObject>
class QLabel;
class QTableWidget;
class QPushButton;
class SegmentedControl;
class ThemeService;

class DataUsageCard : public QFrame {
  Q_OBJECT
 public:
  explicit DataUsageCard(ThemeService* themeService = nullptr, QWidget* parent = nullptr);
  void updateDataUsage(const QJsonObject& snapshot);
 signals:
  void clearRequested();

 private:
  void    setupUi(ThemeService* themeService);
  void    refreshTable();
  QString formatBytes(qint64 bytes) const;
  QString formatHostLabel(const QString& label) const;

 private:
  SegmentedControl* m_rankingModeSelector = nullptr;
  QPushButton*      m_clearButton         = nullptr;
  QTableWidget*     m_topTable            = nullptr;
  QLabel*           m_emptyLabel          = nullptr;
  QJsonObject       m_snapshot;
};
#endif  // DATAUSAGECARD_H
