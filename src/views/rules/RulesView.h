#ifndef RULESVIEW_H
#define RULESVIEW_H

#include <QList>
#include <QVector>
#include <QWidget>

#include "models/RuleItem.h"
#include "widgets/common/MenuComboBox.h"

class QGridLayout;
class QLabel;
class QLineEdit;
class QEvent;
class QShowEvent;
class QPushButton;
class QScrollArea;
class QFrame;
class ProxyService;
class ThemeService;
class ConfigRepository;
class RuleCard;
class RulesView : public QWidget {
  Q_OBJECT

 public:
  explicit RulesView(ConfigRepository* configRepo, ThemeService* themeService, QWidget* parent = nullptr);
  void setProxyService(ProxyService* service);
  void refresh();

 public slots:
  void updateStyle();

 protected:
  void resizeEvent(QResizeEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;
  void showEvent(QShowEvent* event) override;

 private slots:
  void onSearchChanged();
  void onFilterChanged();
  void onRefreshClicked();
  void onEmptyActionClicked();
  void onAddRuleClicked();

 private:
  void setupUI();
  void applyFilters();
  void updateFilterOptions();
  void sortRules();
  void rebuildCards();
  void layoutCards();
  void updateEmptyState();
  void handleEditRule(const RuleItem& rule);
  void handleDeleteRule(const RuleItem& rule);

  ProxyService* m_proxyService = nullptr;

  QLabel*       m_titleLabel    = nullptr;
  QLabel*       m_subtitleLabel = nullptr;
  QPushButton*  m_refreshBtn    = nullptr;
  QPushButton*  m_addBtn        = nullptr;
  QPushButton*  m_ruleSetBtn    = nullptr;
  QLineEdit*    m_searchEdit    = nullptr;
  MenuComboBox* m_typeFilter    = nullptr;
  MenuComboBox* m_proxyFilter   = nullptr;

  QScrollArea*     m_scrollArea    = nullptr;
  QWidget*         m_gridContainer = nullptr;
  QGridLayout*     m_gridLayout    = nullptr;
  QList<RuleCard*> m_cards;

  QFrame*      m_emptyState  = nullptr;
  QLabel*      m_emptyTitle  = nullptr;
  QPushButton* m_emptyAction = nullptr;

  QVector<RuleItem> m_rules;
  QVector<RuleItem> m_filteredRules;
  bool              m_loading           = false;
  int               m_columnCount       = 0;
  bool              m_skipNextAnimation = false;
  ConfigRepository* m_configRepo        = nullptr;
  ThemeService*     m_themeService      = nullptr;
};
#endif  // RULESVIEW_H
