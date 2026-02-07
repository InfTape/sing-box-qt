#ifndef RULEITEM_H
#define RULEITEM_H
#include <QString>

struct RuleItem {
  QString type;
  QString payload;
  QString proxy;
  QString ruleSet;
  bool    isCustom = false;
};
#endif  // RULEITEM_H
