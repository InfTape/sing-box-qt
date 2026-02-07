#include "SettingsView.h"
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <algorithm>
#include "app/interfaces/ThemeService.h"
#include "storage/ConfigConstants.h"
#include "utils/Logger.h"
#include "utils/settings/SettingsHelpers.h"
#include "views/settings/SettingsController.h"
#include "widgets/common/ElideLineEdit.h"
#include "widgets/common/FlowLayout.h"
#include "widgets/common/MenuComboBox.h"
#include "widgets/common/ToggleSwitch.h"

namespace {
constexpr int kLanguageDefaultIndex       = 1;
constexpr int kSpinBoxHeight              = 36;
constexpr int kControlMinWidth            = 150;
constexpr int kControlMinWidthCompact     = 110;
constexpr int kCardMargin                 = 20;
constexpr int kSectionSpacing             = 12;
constexpr int kCardSpacing                = 16;
constexpr int kGridHorizontalSpacing      = 16;
constexpr int kGridVerticalSpacing        = 12;
constexpr int kGridVerticalCompactSpacing = 10;
constexpr int kToggleCardMarginH          = 16;
constexpr int kToggleCardMarginV          = 10;
constexpr int kToggleCardSpacing          = 30;
constexpr int kProfileToggleHSpacing      = 20;
constexpr int kProfileToggleVSpacing      = 10;
constexpr int kPageMargin                 = 24;
constexpr int kTitleSpacing               = 4;
constexpr int kBypassEditHeight           = 96;
constexpr int kSaveButtonHeight           = 36;
constexpr int kSaveButtonWidth            = 110;
constexpr int kKernelFormSpacing          = 15;
constexpr int kSectionPaddingReserve      = 170;
constexpr int kMinRoutingWrapWidth        = 1200;
constexpr int kMinDnsWrapWidth            = 1180;

class NoWheelSpinBox : public QSpinBox {
 public:
  explicit NoWheelSpinBox(QWidget* parent = nullptr) : QSpinBox(parent) {}

 protected:
  void wheelEvent(QWheelEvent* event) override { event->ignore(); }
};
}  // namespace

SettingsView::SettingsView(ThemeService*       themeService,
                           SettingsController* controller,
                           QWidget*            parent)
    : QWidget(parent),
      m_settingsController(controller ? controller
                                      : new SettingsController(this)),
      m_themeService(themeService) {
  setupUI();
  loadSettings();
  if (m_themeService) {
    connect(m_themeService,
            &ThemeService::themeChanged,
            this,
            &SettingsView::updateStyle);
  }
  // Kernel signals
  if (m_settingsController) {
    connect(m_settingsController,
            &SettingsController::installedInfoReady,
            this,
            &SettingsView::onKernelInstalledReady);
    connect(m_settingsController,
            &SettingsController::releasesReady,
            this,
            &SettingsView::onKernelReleasesReady);
    connect(m_settingsController,
            &SettingsController::latestReady,
            this,
            &SettingsView::onKernelLatestReady);
    connect(m_settingsController,
            &SettingsController::downloadProgress,
            this,
            &SettingsView::onKernelDownloadProgress);
    connect(m_settingsController,
            &SettingsController::statusChanged,
            this,
            &SettingsView::onKernelStatusChanged);
    connect(m_settingsController,
            &SettingsController::finished,
            this,
            &SettingsView::onKernelFinished);
  }
  updateStyle();
}

void SettingsView::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  updateResponsiveUi();
  ensureKernelInfoLoaded();
}

void SettingsView::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  updateResponsiveUi();
}

void SettingsView::ensureKernelInfoLoaded() {
  if (m_kernelInfoLoaded) {
    return;
  }
  m_kernelInfoLoaded = true;
  if (m_settingsController) {
    m_settingsController->refreshInstalledInfo();
    m_settingsController->fetchReleaseList();
  }
}

QLabel* SettingsView::createSectionTitle(const QString& text) {
  QLabel* title = new QLabel(text);
  title->setObjectName("SettingsSectionTitle");
  return title;
}

QFrame* SettingsView::createCard() {
  QFrame* card = new QFrame;
  card->setObjectName("SettingsCard");
  return card;
}

QLabel* SettingsView::createFormLabel(const QString& text) {
  QLabel* label = new QLabel(text);
  label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  label->setWordWrap(true);
  label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  return label;
}

QSpinBox* SettingsView::createSpinBox(int  min,
                                      int  max,
                                      int  value,
                                      bool blockWheel) {
  QSpinBox* spin =
      blockWheel ? static_cast<QSpinBox*>(new NoWheelSpinBox) : new QSpinBox;
  spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
  spin->setRange(min, max);
  spin->setValue(value);
  spin->setMinimumWidth(kControlMinWidth);
  spin->setFixedHeight(kSpinBoxHeight);
  spin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  return spin;
}

MenuComboBox* SettingsView::createMenuComboBox(bool expanding) {
  MenuComboBox* combo = new MenuComboBox(this, m_themeService);
  combo->setWheelEnabled(false);
  combo->setFixedHeight(kSpinBoxHeight);
  combo->setMinimumWidth(kControlMinWidth);
  if (expanding) {
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  }
  return combo;
}

ElideLineEdit* SettingsView::createElideLineEdit(const QString& placeholder) {
  ElideLineEdit* edit = new ElideLineEdit;
  edit->setPlaceholderText(placeholder);
  edit->setFixedHeight(kSpinBoxHeight);
  edit->setMinimumWidth(kControlMinWidth);
  return edit;
}

void SettingsView::prepareFormLabelPair(QLabel* left, QLabel* right) {
  if (!left || !right) {
    return;
  }
  // Keep labels flexible so narrow windows can reflow instead of clipping.
  left->setMinimumWidth(0);
  right->setMinimumWidth(0);
  left->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  right->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
}

QWidget* SettingsView::buildProxySection() {
  QWidget*     proxySection       = new QWidget;
  QVBoxLayout* proxySectionLayout = new QVBoxLayout(proxySection);
  proxySectionLayout->setContentsMargins(0, 0, 0, 0);
  proxySectionLayout->setSpacing(kSectionSpacing);
  proxySectionLayout->addWidget(createSectionTitle(tr("Proxy Settings")));
  QFrame*      proxyCard   = createCard();
  QGridLayout* proxyLayout = new QGridLayout(proxyCard);
  proxyLayout->setContentsMargins(
      kCardMargin, kCardMargin, kCardMargin, kCardMargin);
  proxyLayout->setHorizontalSpacing(kGridHorizontalSpacing);
  proxyLayout->setVerticalSpacing(kGridVerticalSpacing);
  proxyLayout->setColumnStretch(1, 1);
  proxyLayout->setColumnStretch(3, 1);
  m_mixedPortSpin        = createSpinBox(1, 65535, 7890, false);
  m_apiPortSpin          = createSpinBox(1, 65535, 9090, false);
  m_autoStartCheck       = new QCheckBox(tr("Auto start on boot"));
  QLabel* mixedPortLabel = createFormLabel(tr("Mixed port:"));
  QLabel* apiPortLabel   = createFormLabel(tr("API port:"));
  prepareFormLabelPair(mixedPortLabel, apiPortLabel);
  proxyLayout->addWidget(mixedPortLabel, 0, 0);
  proxyLayout->addWidget(m_mixedPortSpin, 0, 1);
  proxyLayout->addWidget(apiPortLabel, 0, 2);
  proxyLayout->addWidget(m_apiPortSpin, 0, 3);
  proxyLayout->addWidget(m_autoStartCheck, 1, 0, 1, 4);
  proxySectionLayout->addWidget(proxyCard);
  return proxySection;
}

QWidget* SettingsView::buildProxyAdvancedSection() {
  ThemeService* ts = m_themeService;
  Q_UNUSED(ts);  // Colors obtained via QSS
  QWidget*     proxyAdvancedSection = new QWidget;
  QVBoxLayout* proxyAdvancedLayout  = new QVBoxLayout(proxyAdvancedSection);
  proxyAdvancedLayout->setContentsMargins(0, 0, 0, 0);
  proxyAdvancedLayout->setSpacing(kSectionSpacing);
  proxyAdvancedLayout->addWidget(
      createSectionTitle(tr("Proxy Advanced Settings")));
  QFrame*      proxyAdvancedCard = createCard();
  QVBoxLayout* advancedLayout    = new QVBoxLayout(proxyAdvancedCard);
  advancedLayout->setContentsMargins(
      kCardMargin, kCardMargin, kCardMargin, kCardMargin);
  advancedLayout->setSpacing(kCardSpacing);
  QLabel* bypassLabel     = new QLabel(tr("System proxy bypass domains"));
  m_systemProxyBypassEdit = new QPlainTextEdit;
  m_systemProxyBypassEdit->setPlaceholderText(
      ConfigConstants::DEFAULT_SYSTEM_PROXY_BYPASS);
  m_systemProxyBypassEdit->setFixedHeight(kBypassEditHeight);
  m_systemProxyBypassEdit->setSizePolicy(QSizePolicy::Expanding,
                                         QSizePolicy::Fixed);
  advancedLayout->addWidget(bypassLabel);
  advancedLayout->addWidget(m_systemProxyBypassEdit);
  QLabel* tunTitle = new QLabel(tr("TUN Virtual Adapter"));
  advancedLayout->addWidget(tunTitle);
  QGridLayout* tunGrid = new QGridLayout;
  tunGrid->setHorizontalSpacing(kGridHorizontalSpacing);
  tunGrid->setVerticalSpacing(kGridVerticalCompactSpacing);
  tunGrid->setColumnStretch(1, 1);
  tunGrid->setColumnStretch(3, 1);
  m_tunMtuSpin =
      createSpinBox(576, 9000, ConfigConstants::DEFAULT_TUN_MTU, true);
  m_tunStackCombo = createMenuComboBox(true);
  m_tunStackCombo->addItems({tr("Mixed"), tr("System"), tr("gVisor")});
  QLabel* mtuLabel   = createFormLabel(tr("MTU:"));
  QLabel* stackLabel = createFormLabel(tr("Protocol stack:"));
  tunGrid->addWidget(mtuLabel, 0, 0);
  tunGrid->addWidget(m_tunMtuSpin, 0, 1);
  tunGrid->addWidget(stackLabel, 0, 2);
  tunGrid->addWidget(m_tunStackCombo, 0, 3);
  advancedLayout->addLayout(tunGrid);
  QWidget* toggleCard = new QWidget;
  toggleCard->setObjectName("SettingsToggleCard");
  QHBoxLayout* toggleLayout = new QHBoxLayout(toggleCard);
  toggleLayout->setContentsMargins(kToggleCardMarginH,
                                   kToggleCardMarginV,
                                   kToggleCardMarginH,
                                   kToggleCardMarginV);
  toggleLayout->setSpacing(kToggleCardSpacing);
  auto addToggle = [this, toggleLayout](const QString& text,
                                        ToggleSwitch*& toggle) {
    QWidget*     item       = new QWidget;
    QHBoxLayout* itemLayout = new QHBoxLayout(item);
    itemLayout->setContentsMargins(0, 0, 0, 0);
    itemLayout->setSpacing(10);
    QLabel* label = new QLabel(text);
    toggle        = new ToggleSwitch(this, m_themeService);
    itemLayout->addWidget(label);
    itemLayout->addWidget(toggle);
    itemLayout->addStretch();
    toggleLayout->addWidget(item);
  };
  addToggle(tr("Enable IPv6"), m_tunEnableIpv6Switch);
  addToggle(tr("Auto route"), m_tunAutoRouteSwitch);
  addToggle(tr("Strict route"), m_tunStrictRouteSwitch);
  toggleLayout->addStretch();
  advancedLayout->addWidget(toggleCard);
  QLabel* advancedHint =
      new QLabel(tr("Changes take effect after restart or proxy re-enable."));
  advancedHint->setObjectName("SettingsHint");
  advancedLayout->addWidget(advancedHint);
  proxyAdvancedLayout->addWidget(proxyAdvancedCard);
  return proxyAdvancedSection;
}

QWidget* SettingsView::buildProfileSection() {
  ThemeService* ts = m_themeService;
  Q_UNUSED(ts);  // Colors obtained via QSS
  QWidget*     singboxProfileSection = new QWidget;
  QVBoxLayout* singboxProfileLayout  = new QVBoxLayout(singboxProfileSection);
  singboxProfileLayout->setContentsMargins(0, 0, 0, 0);
  singboxProfileLayout->setSpacing(kSectionSpacing);
  singboxProfileLayout->addWidget(
      createSectionTitle(tr("Subscription Config Profile (Advanced)")));
  QFrame*      singboxProfileCard       = createCard();
  QVBoxLayout* singboxProfileCardLayout = new QVBoxLayout(singboxProfileCard);
  singboxProfileCardLayout->setContentsMargins(
      kCardMargin, kCardMargin, kCardMargin, kCardMargin);
  singboxProfileCardLayout->setSpacing(kCardSpacing);
  QLabel* routingTitle = new QLabel(tr("Routing & Downloads"));
  routingTitle->setProperty("class", "SettingsSectionSubTitle");
  singboxProfileCardLayout->addWidget(routingTitle);
  QGridLayout* routingGrid = new QGridLayout;
  routingGrid->setHorizontalSpacing(kGridHorizontalSpacing);
  routingGrid->setVerticalSpacing(kGridVerticalSpacing);
  routingGrid->setColumnStretch(1, 1);
  routingGrid->setColumnStretch(3, 1);
  m_defaultOutboundCombo = createMenuComboBox();
  m_defaultOutboundCombo->addItems(
      {tr("Manual selector"), tr("Auto select (URLTest)")});
  m_downloadDetourCombo = createMenuComboBox();
  m_downloadDetourCombo->addItems({tr("Manual selector"), tr("Direct")});
  m_defaultOutboundLabel =
      createFormLabel(tr("Default outbound for non-CN traffic"));
  m_downloadDetourLabel = createFormLabel(tr("Rule-set/UI download detour"));
  prepareFormLabelPair(m_defaultOutboundLabel, m_downloadDetourLabel);
  routingGrid->addWidget(m_defaultOutboundLabel, 0, 0);
  routingGrid->addWidget(m_defaultOutboundCombo, 0, 1);
  routingGrid->addWidget(m_downloadDetourLabel, 0, 2);
  routingGrid->addWidget(m_downloadDetourCombo, 0, 3);
  singboxProfileCardLayout->addLayout(routingGrid);
  QWidget* profileToggleCard = new QWidget;
  profileToggleCard->setObjectName("SettingsToggleCard");
  FlowLayout* profileToggleLayout = new FlowLayout(
      profileToggleCard, 0, kProfileToggleHSpacing, kProfileToggleVSpacing);
  profileToggleLayout->setContentsMargins(kToggleCardMarginH,
                                          kToggleCardMarginV,
                                          kToggleCardMarginH,
                                          kToggleCardMarginV);
  auto addProfileToggle = [this, profileToggleLayout](const QString& text,
                                                      ToggleSwitch*& toggle) {
    QWidget*     item       = new QWidget;
    QHBoxLayout* itemLayout = new QHBoxLayout(item);
    itemLayout->setContentsMargins(0, 0, 0, 0);
    itemLayout->setSpacing(10);
    QLabel* label = new QLabel(text);
    toggle        = new ToggleSwitch(this, m_themeService);
    itemLayout->addWidget(label);
    itemLayout->addWidget(toggle);
    itemLayout->addStretch();
    profileToggleLayout->addWidget(item);
  };
  addProfileToggle(tr("Block ads (geosite-category-ads-all)"),
                   m_blockAdsSwitch);
  addProfileToggle(tr("DNS hijack (hijack-dns)"), m_dnsHijackSwitch);
  addProfileToggle(tr("Enable app groups (TG/YouTube/Netflix/OpenAI)"),
                   m_enableAppGroupsSwitch);
  singboxProfileCardLayout->addWidget(profileToggleCard);
  QLabel* dnsTitle = new QLabel(tr("DNS"));
  dnsTitle->setProperty("class", "SettingsSectionSubTitle");
  singboxProfileCardLayout->addWidget(dnsTitle);
  QGridLayout* dnsGrid = new QGridLayout;
  dnsGrid->setHorizontalSpacing(kGridHorizontalSpacing);
  dnsGrid->setVerticalSpacing(kGridVerticalSpacing);
  dnsGrid->setColumnStretch(1, 1);
  dnsGrid->setColumnStretch(3, 1);
  m_dnsProxyEdit = createElideLineEdit(ConfigConstants::DEFAULT_DNS_PROXY);
  m_dnsCnEdit    = createElideLineEdit(ConfigConstants::DEFAULT_DNS_CN);
  m_dnsResolverEdit =
      createElideLineEdit(ConfigConstants::DEFAULT_DNS_RESOLVER);
  m_urltestUrlEdit = createElideLineEdit(ConfigConstants::DEFAULT_URLTEST_URL);
  QLabel* dnsProxyLabel = createFormLabel(tr("Proxy DNS (non-CN)"));
  QLabel* dnsCnLabel    = createFormLabel(tr("CN DNS"));
  prepareFormLabelPair(dnsProxyLabel, dnsCnLabel);
  m_dnsResolverLabel =
      createFormLabel(tr("Resolver DNS (for DoH hostname resolving)"));
  m_urltestLabel = createFormLabel(tr("URLTest URL"));
  prepareFormLabelPair(m_dnsResolverLabel, m_urltestLabel);
  dnsGrid->addWidget(dnsProxyLabel, 0, 0);
  dnsGrid->addWidget(m_dnsProxyEdit, 0, 1);
  dnsGrid->addWidget(dnsCnLabel, 0, 2);
  dnsGrid->addWidget(m_dnsCnEdit, 0, 3);
  dnsGrid->addWidget(m_dnsResolverLabel, 1, 0);
  dnsGrid->addWidget(m_dnsResolverEdit, 1, 1);
  dnsGrid->addWidget(m_urltestLabel, 1, 2);
  dnsGrid->addWidget(m_urltestUrlEdit, 1, 3);
  singboxProfileCardLayout->addLayout(dnsGrid);
  singboxProfileLayout->addWidget(singboxProfileCard);
  return singboxProfileSection;
}

QWidget* SettingsView::buildAppearanceSection() {
  QWidget*     appearanceSection       = new QWidget;
  QVBoxLayout* appearanceSectionLayout = new QVBoxLayout(appearanceSection);
  appearanceSectionLayout->setContentsMargins(0, 0, 0, 0);
  appearanceSectionLayout->setSpacing(kSectionSpacing);
  appearanceSectionLayout->addWidget(createSectionTitle(tr("Appearance")));
  QFrame*      appearanceCard   = createCard();
  QGridLayout* appearanceLayout = new QGridLayout(appearanceCard);
  appearanceLayout->setContentsMargins(
      kCardMargin, kCardMargin, kCardMargin, kCardMargin);
  appearanceLayout->setHorizontalSpacing(kGridHorizontalSpacing);
  appearanceLayout->setVerticalSpacing(kGridVerticalSpacing);
  appearanceLayout->setColumnStretch(1, 1);
  appearanceLayout->setColumnStretch(3, 1);
  QLabel* themeLabel    = createFormLabel(tr("Theme:"));
  QLabel* languageLabel = createFormLabel(tr("Language:"));
  prepareFormLabelPair(themeLabel, languageLabel);
  m_themeCombo = createMenuComboBox(true);
  m_themeCombo->addItems({tr("Dark"), tr("Light"), tr("Follow System")});
  m_languageCombo = createMenuComboBox(true);
  m_languageCombo->addItems(
      {tr("Simplified Chinese"), "English", tr("Japanese"), tr("Russian")});
  appearanceLayout->addWidget(themeLabel, 0, 0);
  appearanceLayout->addWidget(m_themeCombo, 0, 1);
  appearanceLayout->addWidget(languageLabel, 0, 2);
  appearanceLayout->addWidget(m_languageCombo, 0, 3);
  appearanceSectionLayout->addWidget(appearanceCard);
  return appearanceSection;
}

QWidget* SettingsView::buildKernelSection() {
  ThemeService* ts = m_themeService;
  Q_UNUSED(ts);  // Colors obtained via QSS
  QWidget*     kernelSection       = new QWidget;
  QVBoxLayout* kernelSectionLayout = new QVBoxLayout(kernelSection);
  kernelSectionLayout->setContentsMargins(0, 0, 0, 0);
  kernelSectionLayout->setSpacing(kSectionSpacing);
  kernelSectionLayout->addWidget(createSectionTitle(tr("Kernel Settings")));
  QFrame*      kernelCard   = createCard();
  QFormLayout* kernelLayout = new QFormLayout(kernelCard);
  kernelLayout->setContentsMargins(
      kCardMargin, kCardMargin, kCardMargin, kCardMargin);
  kernelLayout->setSpacing(kKernelFormSpacing);
  kernelLayout->setLabelAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  m_kernelVersionLabel = new QLabel(tr("Not installed"));
  m_kernelVersionLabel->setObjectName("KernelVersionLabel");
  m_kernelVersionLabel->setProperty("status", "error");
  m_kernelVersionCombo = createMenuComboBox();
  m_kernelVersionCombo->addItem(tr("Latest version"));
  m_kernelPathEdit = createElideLineEdit(tr("Kernel path"));
  m_kernelPathEdit->setReadOnly(true);
  m_kernelDownloadProgress = new QProgressBar;
  m_kernelDownloadProgress->setObjectName("KernelProgress");
  m_kernelDownloadProgress->setRange(0, 100);
  m_kernelDownloadProgress->setValue(0);
  m_kernelDownloadProgress->setTextVisible(true);
  m_kernelDownloadProgress->setVisible(false);
  m_kernelDownloadStatus = new QLabel;
  m_kernelDownloadStatus->setObjectName("KernelStatusLabel");
  m_kernelDownloadStatus->setVisible(false);
  QHBoxLayout* kernelBtnLayout = new QHBoxLayout;
  m_downloadKernelBtn          = new QPushButton(tr("Download Kernel"));
  m_downloadKernelBtn->setObjectName("DownloadKernelBtn");
  m_checkKernelBtn = new QPushButton(tr("Check Installation"));
  m_checkKernelBtn->setObjectName("CheckKernelBtn");
  m_checkUpdateBtn = new QPushButton(tr("Check Updates"));
  m_checkUpdateBtn->setObjectName("CheckUpdateBtn");
  kernelBtnLayout->addWidget(m_downloadKernelBtn);
  kernelBtnLayout->addWidget(m_checkKernelBtn);
  kernelBtnLayout->addWidget(m_checkUpdateBtn);
  kernelBtnLayout->addStretch();
  kernelLayout->addRow(createFormLabel(tr("Installed version:")),
                       m_kernelVersionLabel);
  kernelLayout->addRow(createFormLabel(tr("Select version:")),
                       m_kernelVersionCombo);
  kernelLayout->addRow(createFormLabel(tr("Kernel path:")), m_kernelPathEdit);
  kernelLayout->addRow(m_kernelDownloadProgress);
  kernelLayout->addRow(m_kernelDownloadStatus);
  kernelLayout->addRow(kernelBtnLayout);
  kernelSectionLayout->addWidget(kernelCard);
  return kernelSection;
}

void SettingsView::setupUI() {
  ThemeService* ts = m_themeService;
  Q_UNUSED(ts);  // Colors obtained via QSS
  // Use global QSS, no longer splicing control styles individually.
  m_inputStyleApplied.clear();
  m_comboStyle.clear();
  QVBoxLayout* outerLayout = new QVBoxLayout(this);
  outerLayout->setContentsMargins(0, 0, 0, 0);
  outerLayout->setSpacing(0);
  QScrollArea* scrollArea = new QScrollArea;
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setObjectName("SettingsScroll");
  QWidget*     contentWidget = new QWidget;
  QVBoxLayout* mainLayout    = new QVBoxLayout(contentWidget);
  mainLayout->setContentsMargins(
      kPageMargin, kPageMargin, kPageMargin, kPageMargin);
  mainLayout->setSpacing(kCardSpacing);
  // Header (match Rules page layout spacing)
  QHBoxLayout* headerLayout = new QHBoxLayout;
  QVBoxLayout* titleLayout  = new QVBoxLayout;
  titleLayout->setSpacing(kTitleSpacing);
  QLabel* titleLabel = new QLabel(tr("Settings"));
  titleLabel->setObjectName("PageTitle");
  QLabel* subtitleLabel = new QLabel(tr("Configure application preferences"));
  subtitleLabel->setObjectName("PageSubtitle");
  titleLayout->addWidget(titleLabel);
  titleLayout->addWidget(subtitleLabel);
  headerLayout->addLayout(titleLayout);
  headerLayout->addStretch();
  mainLayout->addLayout(headerLayout);
  mainLayout->addWidget(buildProxySection());
  mainLayout->addWidget(buildProxyAdvancedSection());
  mainLayout->addWidget(buildProfileSection());
  mainLayout->addWidget(buildAppearanceSection());
  mainLayout->addWidget(buildKernelSection());
  mainLayout->addStretch();
  m_saveBtn = new QPushButton(tr("Save"));
  m_saveBtn->setObjectName("SaveBtn");
  m_saveBtn->setFixedHeight(kSaveButtonHeight);
  m_saveBtn->setFixedWidth(kSaveButtonWidth);
  mainLayout->addWidget(m_saveBtn, 0, Qt::AlignHCenter);
  scrollArea->setWidget(contentWidget);
  outerLayout->addWidget(scrollArea, 1);
  connect(m_saveBtn, &QPushButton::clicked, this, &SettingsView::onSaveClicked);
  connect(m_downloadKernelBtn,
          &QPushButton::clicked,
          this,
          &SettingsView::onDownloadKernelClicked);
  connect(m_checkKernelBtn,
          &QPushButton::clicked,
          this,
          &SettingsView::onCheckKernelClicked);
  connect(m_checkUpdateBtn,
          &QPushButton::clicked,
          this,
          &SettingsView::onCheckUpdateClicked);
  connect(m_themeCombo,
          QOverload<int>::of(&QComboBox::currentIndexChanged),
          this,
          [this](int index) {
            if (m_themeService) {
              m_themeService->setThemeMode(
                  SettingsHelpers::themeModeFromIndex(index));
            }
          });
  connect(m_languageCombo,
          QOverload<int>::of(&QComboBox::activated),
          this,
          [this](int index) {
            if (index == kLanguageDefaultIndex) {
              return;
            }
            QMessageBox::information(this, tr("Info"), tr("Adapting..."));
            QSignalBlocker blocker(m_languageCombo);
            m_languageCombo->setCurrentIndex(kLanguageDefaultIndex);
          });
  updateResponsiveUi();
}

void SettingsView::updateResponsiveUi() {
  int routingRequiredWidth = 0;
  if (m_defaultOutboundLabel && m_downloadDetourLabel) {
    const int text1 = m_defaultOutboundLabel->fontMetrics().horizontalAdvance(
        m_defaultOutboundLabel->text());
    const int text2 = m_downloadDetourLabel->fontMetrics().horizontalAdvance(
        m_downloadDetourLabel->text());
    const int control1 =
        m_defaultOutboundCombo
            ? std::max(m_defaultOutboundCombo->sizeHint().width(),
                       kControlMinWidth)
            : kControlMinWidth;
    const int control2 =
        m_downloadDetourCombo
            ? std::max(m_downloadDetourCombo->sizeHint().width(),
                       kControlMinWidth)
            : kControlMinWidth;
    routingRequiredWidth =
        std::max(text1 + text2 + control1 + control2 +
                     kGridHorizontalSpacing * 3 + kSectionPaddingReserve,
                 kMinRoutingWrapWidth);
  }
  const bool narrow =
      routingRequiredWidth > 0 && width() < routingRequiredWidth;
  auto applyRoutingLabelMode = [narrow](QLabel* label) {
    if (!label) {
      return;
    }
    label->setWordWrap(narrow);
    label->setSizePolicy(narrow ? QSizePolicy::Preferred : QSizePolicy::Minimum,
                         narrow ? QSizePolicy::Minimum : QSizePolicy::Fixed);
    label->updateGeometry();
  };
  applyRoutingLabelMode(m_defaultOutboundLabel);
  applyRoutingLabelMode(m_downloadDetourLabel);
  const int compactMinWidth =
      narrow ? kControlMinWidthCompact : kControlMinWidth;
  if (m_mixedPortSpin) {
    m_mixedPortSpin->setMinimumWidth(compactMinWidth);
  }
  if (m_apiPortSpin) {
    m_apiPortSpin->setMinimumWidth(compactMinWidth);
  }
  if (m_tunMtuSpin) {
    m_tunMtuSpin->setMinimumWidth(compactMinWidth);
  }
  if (m_tunStackCombo) {
    m_tunStackCombo->setMinimumWidth(compactMinWidth);
  }
  if (m_defaultOutboundCombo) {
    m_defaultOutboundCombo->setMinimumWidth(compactMinWidth);
  }
  if (m_downloadDetourCombo) {
    m_downloadDetourCombo->setMinimumWidth(compactMinWidth);
  }
  int dnsRequiredWidth = 0;
  if (m_dnsResolverLabel && m_urltestLabel) {
    const int dnsText1 = m_dnsResolverLabel->fontMetrics().horizontalAdvance(
        m_dnsResolverLabel->text());
    const int dnsText2 =
        m_urltestLabel->fontMetrics().horizontalAdvance(m_urltestLabel->text());
    const int dnsControl1 =
        m_dnsResolverEdit
            ? std::max(m_dnsResolverEdit->sizeHint().width(), compactMinWidth)
            : compactMinWidth;
    const int dnsControl2 =
        m_urltestUrlEdit
            ? std::max(m_urltestUrlEdit->sizeHint().width(), compactMinWidth)
            : compactMinWidth;
    dnsRequiredWidth =
        std::max(dnsText1 + dnsText2 + dnsControl1 + dnsControl2 +
                     kGridHorizontalSpacing * 3 + kSectionPaddingReserve,
                 kMinDnsWrapWidth);
  }
  const bool dnsNarrow = dnsRequiredWidth > 0 && width() < dnsRequiredWidth;
  auto       applyDnsLabelMode = [dnsNarrow](QLabel* label) {
    if (!label) {
      return;
    }
    label->setWordWrap(dnsNarrow);
    label->setSizePolicy(
        dnsNarrow ? QSizePolicy::Preferred : QSizePolicy::Minimum,
        dnsNarrow ? QSizePolicy::Minimum : QSizePolicy::Fixed);
    label->updateGeometry();
  };
  applyDnsLabelMode(m_dnsResolverLabel);
  applyDnsLabelMode(m_urltestLabel);
}

void SettingsView::updateStyle() {
  ThemeService* ts = m_themeService;
  if (!ts) {
    return;
  }
  setStyleSheet(ts->loadStyleSheet(":/styles/settings_view.qss"));
  {
    QSignalBlocker blocker(m_themeCombo);
    if (m_themeCombo) {
      m_themeCombo->setCurrentIndex(
          SettingsHelpers::themeIndexFromMode(ts->themeMode()));
    }
  }
}

QString SettingsView::normalizeBypassText(const QString& text) const {
  return SettingsHelpers::normalizeBypassText(text);
}

void SettingsView::fillGeneralFromUi(SettingsModel::Data& data) const {
  data.mixedPort = m_mixedPortSpin->value();
  data.apiPort   = m_apiPortSpin->value();
  data.autoStart = m_autoStartCheck->isChecked();
}

void SettingsView::fillAdvancedFromUi(SettingsModel::Data& data) const {
  data.systemProxyBypass =
      normalizeBypassText(m_systemProxyBypassEdit->toPlainText());
  data.tunMtu = m_tunMtuSpin->value();
  switch (m_tunStackCombo->currentIndex()) {
    case 1:
      data.tunStack = "system";
      break;
    case 2:
      data.tunStack = "gvisor";
      break;
    default:
      data.tunStack = "mixed";
      break;
  }
  data.tunEnableIpv6  = m_tunEnableIpv6Switch->isChecked();
  data.tunAutoRoute   = m_tunAutoRouteSwitch->isChecked();
  data.tunStrictRoute = m_tunStrictRouteSwitch->isChecked();
}

void SettingsView::fillProfileFromUi(SettingsModel::Data& data) const {
  data.defaultOutbound =
      (m_defaultOutboundCombo->currentIndex() == 1) ? "auto" : "manual";
  data.downloadDetour =
      (m_downloadDetourCombo->currentIndex() == 0) ? "manual" : "direct";
  data.blockAds        = m_blockAdsSwitch->isChecked();
  data.dnsHijack       = m_dnsHijackSwitch->isChecked();
  data.enableAppGroups = m_enableAppGroupsSwitch->isChecked();
  data.dnsProxy        = SettingsHelpers::resolveTextOrDefault(
      m_dnsProxyEdit, ConfigConstants::DEFAULT_DNS_PROXY);
  data.dnsCn = SettingsHelpers::resolveTextOrDefault(
      m_dnsCnEdit, ConfigConstants::DEFAULT_DNS_CN);
  data.dnsResolver = SettingsHelpers::resolveTextOrDefault(
      m_dnsResolverEdit, ConfigConstants::DEFAULT_DNS_RESOLVER);
  data.urltestUrl = SettingsHelpers::resolveTextOrDefault(
      m_urltestUrlEdit, ConfigConstants::DEFAULT_URLTEST_URL);
}

void SettingsView::applySettingsToUi(const SettingsModel::Data& data) {
  m_mixedPortSpin->setValue(data.mixedPort);
  m_apiPortSpin->setValue(data.apiPort);
  m_autoStartCheck->setChecked(data.autoStart);
  m_systemProxyBypassEdit->setPlainText(data.systemProxyBypass);
  m_tunMtuSpin->setValue(data.tunMtu);
  if (data.tunStack == "system") {
    m_tunStackCombo->setCurrentIndex(1);
  } else if (data.tunStack == "gvisor") {
    m_tunStackCombo->setCurrentIndex(2);
  } else {
    m_tunStackCombo->setCurrentIndex(0);
  }
  m_tunEnableIpv6Switch->setChecked(data.tunEnableIpv6);
  m_tunAutoRouteSwitch->setChecked(data.tunAutoRoute);
  m_tunStrictRouteSwitch->setChecked(data.tunStrictRoute);
  m_defaultOutboundCombo->setCurrentIndex(data.defaultOutbound == "auto" ? 1
                                                                         : 0);
  m_downloadDetourCombo->setCurrentIndex(data.downloadDetour == "manual" ? 0
                                                                         : 1);
  m_blockAdsSwitch->setChecked(data.blockAds);
  m_dnsHijackSwitch->setChecked(data.dnsHijack);
  m_enableAppGroupsSwitch->setChecked(data.enableAppGroups);
  m_dnsProxyEdit->setText(data.dnsProxy);
  m_dnsCnEdit->setText(data.dnsCn);
  m_dnsResolverEdit->setText(data.dnsResolver);
  m_urltestUrlEdit->setText(data.urltestUrl);
  {
    QSignalBlocker blocker(m_themeCombo);
    if (m_themeCombo) {
      const ThemeService::ThemeMode mode = m_themeService
                                               ? m_themeService->themeMode()
                                               : ThemeService::ThemeMode::Dark;
      m_themeCombo->setCurrentIndex(SettingsHelpers::themeIndexFromMode(mode));
    }
  }
  {
    QSignalBlocker blocker(m_languageCombo);
    m_languageCombo->setCurrentIndex(kLanguageDefaultIndex);
  }
}

void SettingsView::loadSettings() {
  if (m_settingsController) {
    applySettingsToUi(m_settingsController->loadSettings());
  }
}

bool SettingsView::saveSettings() {
  if (!m_settingsController) {
    return false;
  }
  SettingsModel::Data data = m_settingsController->loadSettings();
  fillGeneralFromUi(data);
  fillAdvancedFromUi(data);
  fillProfileFromUi(data);
  QString errorMessage;
  if (!m_settingsController->saveSettings(data,
                                          m_themeCombo->currentIndex(),
                                          m_languageCombo->currentIndex(),
                                          &errorMessage)) {
    QMessageBox::warning(this, tr("Notice"), errorMessage);
    return false;
  }
  return true;
}

void SettingsView::onSaveClicked() {
  if (saveSettings()) {
    QMessageBox::information(this, tr("Notice"), tr("Settings saved"));
  }
}

void SettingsView::onDownloadKernelClicked() {
  if (m_isDownloading || !m_settingsController) {
    return;
  }
  QString version;
  if (m_kernelVersionCombo && m_kernelVersionCombo->currentIndex() > 0) {
    version = m_kernelVersionCombo->currentText().trimmed();
  }
  setDownloadUi(true, tr("Preparing to download kernel..."));
  m_settingsController->downloadAndInstall(version);
}

void SettingsView::onCheckKernelClicked() {
  if (m_settingsController) {
    m_checkingInstall = true;
    setDownloadUi(true, tr("Checking installation..."));
    m_settingsController->refreshInstalledInfo();
    m_settingsController->fetchReleaseList();
  } else {
    setDownloadUi(false, tr("Kernel manager unavailable"));
  }
}

void SettingsView::onCheckUpdateClicked() {
  if (m_settingsController) {
    setDownloadUi(true, tr("Checking latest kernel version..."));
    m_settingsController->checkLatest();
  }
}

void SettingsView::onKernelInstalledReady(const QString& path,
                                          const QString& version) {
  setDownloadUi(false, tr("Installation check finished"));
  m_installedKernelVersion = version.trimmed();
  if (m_kernelPathEdit) {
    m_kernelPathEdit->setText(path);
  }
  if (m_kernelVersionLabel) {
    if (m_installedKernelVersion.isEmpty()) {
      m_kernelVersionLabel->setText(tr("Not installed"));
    } else {
      m_kernelVersionLabel->setText(m_installedKernelVersion);
    }
  }
  updateKernelVersionLabelStatus();
  if (m_checkingInstall) {
    m_checkingInstall = false;
    const QString msg = m_installedKernelVersion.isEmpty()
                            ? tr("Kernel is not installed.")
                            : tr("Kernel installed. Version: %1\nPath: %2")
                                  .arg(m_installedKernelVersion,
                                       path.isEmpty() ? tr("Unknown") : path);
    QMessageBox::information(this, tr("Check Installation"), msg);
  }
}

void SettingsView::onKernelReleasesReady(const QStringList& versions,
                                         const QString&     latest) {
  setDownloadUi(false);
  m_latestKernelVersion = latest.trimmed();
  updateKernelVersionLabelStatus();
  if (!m_kernelVersionCombo) {
    return;
  }
  m_kernelVersionCombo->clear();
  m_kernelVersionCombo->addItem(tr("Latest version"));
  for (const QString& ver : versions) {
    m_kernelVersionCombo->addItem(ver);
  }
}

void SettingsView::onKernelLatestReady(const QString& latest,
                                       const QString& installed) {
  setDownloadUi(false);
  m_latestKernelVersion    = latest.trimmed();
  m_installedKernelVersion = installed.trimmed();
  if (m_kernelVersionLabel) {
    if (m_installedKernelVersion.isEmpty()) {
      m_kernelVersionLabel->setText(tr("Not installed"));
    } else {
      m_kernelVersionLabel->setText(m_installedKernelVersion);
    }
  }
  updateKernelVersionLabelStatus();
  if (m_installedKernelVersion.isEmpty()) {
    QMessageBox::information(this,
                             tr("Check Updates"),
                             tr("Kernel not installed. Latest version is %1")
                                 .arg(m_latestKernelVersion));
    return;
  }
  if (m_installedKernelVersion == m_latestKernelVersion) {
    QMessageBox::information(
        this, tr("Check Updates"), tr("Already on the latest version"));
  } else {
    QMessageBox::information(
        this,
        tr("Check Updates"),
        tr("New kernel version %1 available, current %2")
            .arg(m_latestKernelVersion, m_installedKernelVersion));
  }
}

void SettingsView::updateKernelVersionLabelStatus() {
  if (!m_kernelVersionLabel) {
    return;
  }
  const bool hasInstalled = !m_installedKernelVersion.isEmpty();
  const bool hasLatest    = !m_latestKernelVersion.isEmpty();
  const bool isLatest     = hasInstalled && hasLatest &&
                        (m_installedKernelVersion == m_latestKernelVersion);
  m_kernelVersionLabel->setProperty("status", isLatest ? "success" : "error");
  m_kernelVersionLabel->style()->unpolish(m_kernelVersionLabel);
  m_kernelVersionLabel->style()->polish(m_kernelVersionLabel);
  m_kernelVersionLabel->update();
}

void SettingsView::onKernelDownloadProgress(int percent) {
  if (m_kernelDownloadProgress) {
    m_kernelDownloadProgress->setValue(percent);
    m_kernelDownloadProgress->setVisible(true);
  }
}

void SettingsView::onKernelStatusChanged(const QString& status) {
  setDownloadUi(true, status);
}

void SettingsView::onKernelFinished(bool ok, const QString& message) {
  setDownloadUi(false, message);
  if (ok) {
    QMessageBox::information(this, tr("Kernel"), message);
  } else {
    QMessageBox::warning(this, tr("Kernel"), message);
  }
}

void SettingsView::setDownloadUi(bool downloading, const QString& message) {
  m_isDownloading = downloading;
  m_downloadKernelBtn->setEnabled(!downloading);
  m_checkKernelBtn->setEnabled(!downloading);
  m_checkUpdateBtn->setEnabled(!downloading);
  m_kernelVersionCombo->setEnabled(!downloading);
  if (downloading) {
    m_kernelDownloadProgress->setValue(0);
    m_kernelDownloadProgress->setVisible(true);
    m_kernelDownloadStatus->setVisible(true);
    if (!message.isEmpty()) {
      m_kernelDownloadStatus->setText(message);
    }
  } else {
    m_kernelDownloadProgress->setVisible(false);
    if (!message.isEmpty()) {
      m_kernelDownloadStatus->setText(message);
      m_kernelDownloadStatus->setVisible(true);
    } else {
      m_kernelDownloadStatus->setVisible(false);
    }
  }
}
