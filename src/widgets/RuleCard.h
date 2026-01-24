#ifndef RULECARD_H
#define RULECARD_H

#include <QFrame>
#include "models/RuleItem.h"

class QLabel;
class RoundedMenu;

class RuleCard : public QFrame
{
    Q_OBJECT

public:
    explicit RuleCard(const RuleItem &rule, int index, QWidget *parent = nullptr);

signals:
    void editRequested(const RuleItem &rule);
    void deleteRequested(const RuleItem &rule);

private:
    void setupUI();
    void updateMenuTheme();
    static QString ruleTagType(const RuleItem &rule);
    static QString proxyTagType(const QString &proxy);

    RuleItem m_rule;
    int m_index = 0;
    RoundedMenu *m_menu = nullptr;
};

#endif // RULECARD_H
