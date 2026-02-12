#include "widgets/rules/RuleCard.h"
#include <QAction>
#include <QFrame>
#include <QLabel>
#include <QPoint>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>
#include "app/interfaces/ThemeService.h"
#include "utils/rule/RuleUtils.h"
#include "widgets/common/RoundedMenu.h"

RuleCard::RuleCard(const RuleItem& rule,
                   int             index,
                   ThemeService*   themeService,
                   QWidget*        parent)
    : QFrame(parent),
      m_rule(rule),
      m_index(index),
      m_themeService(themeService) {
  setupUI();
  updateStyle();
  if (m_themeService) {
    connect(m_themeService,
            &ThemeService::themeChanged,
            this,
            &RuleCard::updateStyle);
  }
}

void RuleCard::setupUI() {
  setObjectName("RuleCard");
  setFrameShape(QFrame::NoFrame);
  setAttribute(Qt::WA_StyledBackground, true);
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(18, 16, 18, 16);
  layout->setSpacing(12);
  QHBoxLayout* headerLayout = new QHBoxLayout;
  headerLayout->setSpacing(6);
  QLabel* statusTag =
      new QLabel(m_rule.isCustom ? tr("Custom") : tr("Built-in"));
  statusTag->setObjectName(m_rule.isCustom ? "CardTagActive" : "CardTag");
  statusTag->setAttribute(Qt::WA_StyledBackground, true);
  statusTag->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  statusTag->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  headerLayout->addWidget(statusTag);
  headerLayout->addStretch();
  if (m_rule.isCustom) {
    QPushButton* menuBtn = new QPushButton("...");
    menuBtn->setFlat(true);
    menuBtn->setCursor(Qt::PointingHandCursor);
    menuBtn->setFixedSize(32, 28);
    menuBtn->setObjectName("RuleMenuBtn");
    m_menu = new RoundedMenu(this);
    m_menu->setObjectName("RuleMenu");
    updateMenuTheme();
    if (m_themeService) {
      connect(m_themeService,
              &ThemeService::themeChanged,
              this,
              &RuleCard::updateMenuTheme);
    }
    QAction* editTypeAct = m_menu->addAction(tr("Edit Match Type"));
    QAction* removeAct   = m_menu->addAction(tr("Delete Rule"));
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
  QFrame* infoPanel = new QFrame(this);
  infoPanel->setObjectName("CardInfoPanel");
  QVBoxLayout* infoLayout = new QVBoxLayout(infoPanel);
  infoLayout->setContentsMargins(12, 10, 12, 10);
  infoLayout->setSpacing(6);
  QLabel* payloadLabel = new QLabel(m_rule.payload, infoPanel);
  payloadLabel->setObjectName("CardInfoText");
  payloadLabel->setWordWrap(true);
  QString typeLabelText = m_rule.ruleSet.trimmed();
  if (!m_rule.isCustom &&
      (typeLabelText.isEmpty() ||
       typeLabelText.compare("default", Qt::CaseInsensitive) == 0)) {
    typeLabelText = tr("Built-in");
  } else if (typeLabelText.isEmpty()) {
    typeLabelText = tr("Default");
  }
  QLabel* typeLabel =
      new QLabel(tr("Rule Set: %1").arg(typeLabelText), infoPanel);
  typeLabel->setObjectName("CardInfoText");
  infoLayout->addWidget(payloadLabel);
  infoLayout->addWidget(typeLabel);
  QPushButton* proxyBtn =
      new QPushButton(RuleUtils::displayProxyLabel(m_rule.proxy), this);
  const QString proxyKey       = RuleUtils::normalizeProxyValue(m_rule.proxy);
  const bool    highlightProxy = proxyKey == "direct" || proxyKey == "manual";
  proxyBtn->setObjectName(highlightProxy ? "CardActionBtnActive"
                                         : "CardActionBtn");
  proxyBtn->setCursor(Qt::ArrowCursor);
  proxyBtn->setFocusPolicy(Qt::NoFocus);
  proxyBtn->setMinimumHeight(38);
  proxyBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  layout->addLayout(headerLayout);
  layout->addWidget(infoPanel);
  layout->addStretch();
  layout->addWidget(proxyBtn);
}

void RuleCard::updateStyle() {
  ThemeService* ts = m_themeService;
  if (!ts) {
    return;
  }
  QString qss = ts->loadStyleSheet(":/styles/card_common.qss");
  setStyleSheet(qss);
}

void RuleCard::updateMenuTheme() {
  if (!m_menu) {
    return;
  }
  ThemeService* ts = m_themeService;
  if (ts) {
    m_menu->setThemeColors(ts->color("bg-secondary"), ts->color("primary"));
  }
}
