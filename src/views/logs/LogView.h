#ifndef LOGVIEW_H
#define LOGVIEW_H
#include <QVector>
#include <QWidget>
#include "utils/LogParser.h"
#include "widgets/common/MenuComboBox.h"
class QCheckBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class ThemeService;

class LogView : public QWidget {
  Q_OBJECT
 public:
  explicit LogView(ThemeService* themeService, QWidget* parent = nullptr);
  void appendApiLog(const QString& type, const QString& payload);
  void clear();
 public slots:
  void updateStyle();
 private slots:
  void onFilterChanged();
  void onClearClicked();
  void onCopyClicked();
  void onExportClicked();

 private:
  void          setupUI();
  void          rebuildList();
  void          updateStats();
  void          updateEmptyState();
  bool          logMatchesFilter(const LogParser::LogEntry& entry) const;
  void          appendLogRow(const LogParser::LogEntry& entry);
  void          removeFirstLogRow();
  void          clearListWidgets();
  QLabel*       m_titleLabel    = nullptr;
  QLabel*       m_subtitleLabel = nullptr;
  QLabel*       m_totalTag      = nullptr;
  QLabel*       m_errorTag      = nullptr;
  QLabel*       m_warningTag    = nullptr;
  QCheckBox*    m_autoScroll    = nullptr;
  QPushButton*  m_clearBtn      = nullptr;
  QPushButton*  m_copyBtn       = nullptr;
  QPushButton*  m_exportBtn     = nullptr;
  QLineEdit*    m_searchEdit    = nullptr;
  MenuComboBox* m_typeFilter    = nullptr;
  QScrollArea*  m_scrollArea    = nullptr;
  QWidget*      m_listContainer = nullptr;
  QVBoxLayout*  m_listLayout    = nullptr;
  QFrame*       m_emptyState    = nullptr;
  QLabel*       m_emptyTitle    = nullptr;
  QVector<LogParser::LogEntry> m_logs;
  QVector<LogParser::LogEntry> m_filtered;
  ThemeService*                m_themeService = nullptr;
};
#endif  // LOGVIEW_H
