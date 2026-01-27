#include "widgets/RuleCard.h"
#include "utils/RuleUtils.h"
#include "utils/ThemeManager.h"
#include "widgets/RoundedMenu.h"
#include <QAction>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPoint>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

RuleCard::RuleCard(const RuleItem &rule, int index, QWidget *parent)
    : QFrame(parent)
    , m_rule(rule)
    , m_index(index)
{
    setupUI();
}

void RuleCard::setupUI()
{
    setObjectName("RuleCard");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    QHBoxLayout *headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(6);

    QLabel *typeTag = new QLabel(m_rule.isCustom ? tr("Custom Rule") : RuleUtils::displayRuleTypeLabel(m_rule.type));
    typeTag->setObjectName("RuleTag");
    typeTag->setProperty("tagType", ruleTagType(m_rule));
    typeTag->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    typeTag->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    const int tagPaddingX = 6;
    const int tagPaddingY = 2;
    const QFontMetrics fm(typeTag->font());
    const QSize textSize = fm.size(Qt::TextSingleLine, typeTag->text());
    typeTag->setFixedSize(textSize.width() + tagPaddingX * 2,
                          textSize.height() + tagPaddingY * 2);

    QLabel *indexLabel = new QLabel(QString("#%1").arg(m_index));
    indexLabel->setObjectName("RuleIndex");

    headerLayout->addWidget(typeTag);
    headerLayout->addStretch();
    if (m_rule.isCustom) {
        QPushButton *menuBtn = new QPushButton("...");
        menuBtn->setFlat(true);
        menuBtn->setCursor(Qt::PointingHandCursor);
        menuBtn->setFixedSize(32, 28);
        menuBtn->setObjectName("RuleMenuBtn");

        m_menu = new RoundedMenu(this);
        m_menu->setObjectName("RuleMenu");
        updateMenuTheme();
        ThemeManager &tm = ThemeManager::instance();
        connect(&tm, &ThemeManager::themeChanged, this, &RuleCard::updateMenuTheme);

        QAction *editTypeAct = m_menu->addAction(tr("Edit Match Type"));
        QAction *removeAct = m_menu->addAction(tr("Delete Rule"));
        removeAct->setObjectName("DeleteAction");

        connect(menuBtn, &QPushButton::clicked, [menuBtn, menu = m_menu]() {
            menu->exec(menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
        });
        connect(editTypeAct, &QAction::triggered, this, [this]() {
            emit editRequested(m_rule);
        });
        connect(removeAct, &QAction::triggered, this, [this]() {
            emit deleteRequested(m_rule);
        });

        headerLayout->addWidget(menuBtn);
    }
    headerLayout->addWidget(indexLabel);

    QVBoxLayout *bodyLayout = new QVBoxLayout;
    bodyLayout->setSpacing(6);

    QLabel *contentValue = new QLabel(m_rule.payload);
    contentValue->setObjectName("RuleValue");
    contentValue->setWordWrap(true);

    QLabel *proxyValue = new QLabel(RuleUtils::displayProxyLabel(m_rule.proxy));
    proxyValue->setObjectName("RuleProxyTag");
    proxyValue->setProperty("tagType", proxyTagType(m_rule.proxy));

    bodyLayout->addWidget(contentValue);
    bodyLayout->addWidget(proxyValue);

    layout->addLayout(headerLayout);
    layout->addLayout(bodyLayout);
}

void RuleCard::updateMenuTheme()
{
    if (!m_menu) return;
    ThemeManager &tm = ThemeManager::instance();
    m_menu->setThemeColors(tm.getColor("bg-secondary"), tm.getColor("primary"));
}

QString RuleCard::ruleTagType(const RuleItem &rule)
{
    if (rule.isCustom) return "info";
    const QString lower = rule.type.toLower();
    if (lower.contains("domain")) return "info";
    if (lower.contains("ipcidr")) return "success";
    if (lower.contains("source")) return "warning";
    if (lower.contains("port")) return "error";
    return "default";
}

QString RuleCard::proxyTagType(const QString &proxy)
{
    const QString value = RuleUtils::normalizeProxyValue(proxy);
    if (value == "direct") return "success";
    if (value == "reject") return "error";
    return "info";
}
