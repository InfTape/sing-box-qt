#ifndef RULECARD_H
#define RULECARD_H
#include <QFrame>
#include "models/RuleItem.h"
class QLabel;
class RoundedMenu;
class ThemeService;

class RuleCard : public QFrame {
  Q_OBJECT
 public:
  explicit RuleCard(const RuleItem& rule,
                    int             index,
                    ThemeService*   themeService,
                    QWidget*        parent = nullptr);
 signals:
  void editRequested(const RuleItem& rule);
  void deleteRequested(const RuleItem& rule);

 private:
  void          setupUI();
  void          updateStyle();
  void          updateMenuTheme();
  RuleItem      m_rule;
  int           m_index        = 0;
  RoundedMenu*  m_menu         = nullptr;
  ThemeService* m_themeService = nullptr;
};
#endif  // RULECARD_H
