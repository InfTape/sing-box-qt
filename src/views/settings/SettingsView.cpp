#include "SettingsView.h"
#include "storage/ConfigConstants.h"
#include "utils/Logger.h"
#include "services/kernel/KernelManager.h"
#include "services/settings/SettingsService.h"
#include "utils/settings/SettingsHelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QScrollArea>
#include <QSizePolicy>
#include <QWheelEvent>
#include <QSignalBlocker>
#include <QShowEvent>
#include <algorithm>
#include "utils/ThemeManager.h"
#include "app/ThemeProvider.h"
#include "widgets/common/MenuComboBox.h"
#include "widgets/common/ToggleSwitch.h"

namespace {

constexpr int kLanguageDefaultIndex = 1;
constexpr int kSpinBoxHeight = 34;

class NoWheelSpinBox : public QSpinBox
{
public:
    explicit NoWheelSpinBox(QWidget *parent = nullptr)
        : QSpinBox(parent)
    {
    }

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        event->ignore();
    }
};
} // namespace

SettingsView::SettingsView(QWidget *parent)
    : QWidget(parent)
    , m_kernelManager(new KernelManager(this))
{
    setupUI();
    loadSettings();

    if (ThemeProvider::instance()) {
        connect(ThemeProvider::instance(), &ThemeService::themeChanged, this, &SettingsView::updateStyle);
    }

    // Kernel signals
    connect(m_kernelManager, &KernelManager::installedInfoReady, this, &SettingsView::onKernelInstalledReady);
    connect(m_kernelManager, &KernelManager::releasesReady, this, &SettingsView::onKernelReleasesReady);
    connect(m_kernelManager, &KernelManager::latestReady, this, &SettingsView::onKernelLatestReady);
    connect(m_kernelManager, &KernelManager::downloadProgress, this, &SettingsView::onKernelDownloadProgress);
    connect(m_kernelManager, &KernelManager::statusChanged, this, &SettingsView::onKernelStatusChanged);
    connect(m_kernelManager, &KernelManager::finished, this, &SettingsView::onKernelFinished);

    updateStyle();
}

void SettingsView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    ensureKernelInfoLoaded();
}

void SettingsView::ensureKernelInfoLoaded()
{
    if (m_kernelInfoLoaded) {
        return;
    }
    m_kernelInfoLoaded = true;
    if (m_kernelManager) {
        m_kernelManager->refreshInstalledInfo();
        m_kernelManager->fetchReleaseList();
    }
}

QLabel* SettingsView::createSectionTitle(const QString &text)
{
    QLabel *title = new QLabel(text);
    title->setObjectName("SettingsSectionTitle");
    return title;
}

QFrame* SettingsView::createCard()
{
    QFrame *card = new QFrame;
    card->setObjectName("SettingsCard");
    return card;
}

QLabel* SettingsView::createFormLabel(const QString &text)
{
    QLabel *label = new QLabel(text);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    return label;
}

void SettingsView::matchLabelWidth(QLabel *left, QLabel *right)
{
    if (!left || !right) {
        return;
    }
    const int width = std::max(left->sizeHint().width(), right->sizeHint().width());
    left->setFixedWidth(width);
    right->setFixedWidth(width);
}

QWidget* SettingsView::buildProxySection()
{
    QWidget *proxySection = new QWidget;
    QVBoxLayout *proxySectionLayout = new QVBoxLayout(proxySection);
    proxySectionLayout->setContentsMargins(0, 0, 0, 0);
    proxySectionLayout->setSpacing(12);

    proxySectionLayout->addWidget(createSectionTitle(tr("Proxy Settings")));

    QFrame *proxyCard = createCard();

    QGridLayout *proxyLayout = new QGridLayout(proxyCard);
    proxyLayout->setContentsMargins(20, 20, 20, 20);
    proxyLayout->setHorizontalSpacing(16);
    proxyLayout->setVerticalSpacing(12);
    proxyLayout->setColumnStretch(1, 1);
    proxyLayout->setColumnStretch(3, 1);

    m_mixedPortSpin = new QSpinBox;
    m_mixedPortSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_mixedPortSpin->setRange(1, 65535);
    m_mixedPortSpin->setValue(7890);
    m_mixedPortSpin->setMinimumWidth(150);
    m_mixedPortSpin->setFixedHeight(kSpinBoxHeight);
    m_mixedPortSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_apiPortSpin = new QSpinBox;
    m_apiPortSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_apiPortSpin->setRange(1, 65535);
    m_apiPortSpin->setValue(9090);
    m_apiPortSpin->setMinimumWidth(150);
    m_apiPortSpin->setFixedHeight(kSpinBoxHeight);
    m_apiPortSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_autoStartCheck = new QCheckBox(tr("Auto start on boot"));

    m_systemProxyCheck = new QCheckBox(tr("Auto-set system proxy"));

    QLabel *mixedPortLabel = createFormLabel(tr("Mixed port:"));
    QLabel *apiPortLabel = createFormLabel(tr("API port:"));
    matchLabelWidth(mixedPortLabel, apiPortLabel);

    proxyLayout->addWidget(mixedPortLabel, 0, 0);
    proxyLayout->addWidget(m_mixedPortSpin, 0, 1);
    proxyLayout->addWidget(apiPortLabel, 0, 2);
    proxyLayout->addWidget(m_apiPortSpin, 0, 3);
    proxyLayout->addWidget(m_autoStartCheck, 1, 0, 1, 4);
    proxyLayout->addWidget(m_systemProxyCheck, 2, 0, 1, 4);

    proxySectionLayout->addWidget(proxyCard);
    return proxySection;
}

QWidget* SettingsView::buildProxyAdvancedSection()
{
    ThemeService *ts = ThemeProvider::instance();
    Q_UNUSED(ts); // Colors obtained via QSS

    QWidget *proxyAdvancedSection = new QWidget;
    QVBoxLayout *proxyAdvancedLayout = new QVBoxLayout(proxyAdvancedSection);
    proxyAdvancedLayout->setContentsMargins(0, 0, 0, 0);
    proxyAdvancedLayout->setSpacing(12);
    proxyAdvancedLayout->addWidget(createSectionTitle(tr("Proxy Advanced Settings")));

    QFrame *proxyAdvancedCard = createCard();
    QVBoxLayout *advancedLayout = new QVBoxLayout(proxyAdvancedCard);
    advancedLayout->setContentsMargins(20, 20, 20, 20);
    advancedLayout->setSpacing(16);

    QLabel *bypassLabel = new QLabel(tr("System proxy bypass domains"));
    m_systemProxyBypassEdit = new QPlainTextEdit;
    m_systemProxyBypassEdit->setPlaceholderText(ConfigConstants::DEFAULT_SYSTEM_PROXY_BYPASS);
    m_systemProxyBypassEdit->setMinimumHeight(80);

    advancedLayout->addWidget(bypassLabel);
    advancedLayout->addWidget(m_systemProxyBypassEdit);

    QLabel *tunTitle = new QLabel(tr("TUN Virtual Adapter"));
    advancedLayout->addWidget(tunTitle);

    QHBoxLayout *tunRow = new QHBoxLayout;
    QFormLayout *tunLeft = new QFormLayout;
    QFormLayout *tunRight = new QFormLayout;
    tunLeft->setSpacing(10);
    tunRight->setSpacing(10);
    tunLeft->setLabelAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    tunRight->setLabelAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    tunLeft->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    tunRight->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_tunMtuSpin = new NoWheelSpinBox;
    m_tunMtuSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_tunMtuSpin->setRange(576, 9000);
    m_tunMtuSpin->setValue(ConfigConstants::DEFAULT_TUN_MTU);
    m_tunMtuSpin->setMinimumWidth(150);
    m_tunMtuSpin->setFixedHeight(kSpinBoxHeight);
    m_tunMtuSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_tunStackCombo = new MenuComboBox;
    m_tunStackCombo->addItems({tr("Mixed"), tr("System"), tr("gVisor")});
    m_tunStackCombo->setWheelEnabled(false);
    m_tunStackCombo->setFixedHeight(kSpinBoxHeight);
    m_tunStackCombo->setMinimumWidth(150);

    QLabel *mtuLabel = createFormLabel(tr("MTU:"));
    QLabel *stackLabel = createFormLabel(tr("Protocol stack:"));
    matchLabelWidth(mtuLabel, stackLabel);

    tunLeft->addRow(mtuLabel, m_tunMtuSpin);
    tunRight->addRow(stackLabel, m_tunStackCombo);
    tunRow->addLayout(tunLeft, 1);
    tunRow->addLayout(tunRight, 1);
    advancedLayout->addLayout(tunRow);

    QWidget *toggleCard = new QWidget;
    toggleCard->setObjectName("SettingsToggleCard");
    QHBoxLayout *toggleLayout = new QHBoxLayout(toggleCard);
    toggleLayout->setContentsMargins(16, 10, 16, 10);
    toggleLayout->setSpacing(30);

    auto addToggle = [toggleLayout](const QString &text, ToggleSwitch *&toggle) {
        QWidget *item = new QWidget;
        QHBoxLayout *itemLayout = new QHBoxLayout(item);
        itemLayout->setContentsMargins(0, 0, 0, 0);
        itemLayout->setSpacing(10);
        QLabel *label = new QLabel(text);
        toggle = new ToggleSwitch;
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

    QLabel *advancedHint = new QLabel(tr("Changes take effect after restart or proxy re-enable."));
    advancedHint->setObjectName("SettingsHint");
    advancedLayout->addWidget(advancedHint);

    proxyAdvancedLayout->addWidget(proxyAdvancedCard);
    return proxyAdvancedSection;
}

QWidget* SettingsView::buildProfileSection()
{
    ThemeService *ts = ThemeProvider::instance();
    Q_UNUSED(ts); // Colors obtained via QSS

    QWidget *singboxProfileSection = new QWidget;
    QVBoxLayout *singboxProfileLayout = new QVBoxLayout(singboxProfileSection);
    singboxProfileLayout->setContentsMargins(0, 0, 0, 0);
    singboxProfileLayout->setSpacing(12);
    singboxProfileLayout->addWidget(createSectionTitle(tr("Subscription Config Profile (Advanced)")));

    QFrame *singboxProfileCard = createCard();
    QVBoxLayout *singboxProfileCardLayout = new QVBoxLayout(singboxProfileCard);
    singboxProfileCardLayout->setContentsMargins(20, 20, 20, 20);
    singboxProfileCardLayout->setSpacing(16);

    QLabel *routingTitle = new QLabel(tr("Routing & Downloads"));
    routingTitle->setProperty("class", "SettingsSectionSubTitle");
    singboxProfileCardLayout->addWidget(routingTitle);

    QGridLayout *routingGrid = new QGridLayout;
    routingGrid->setHorizontalSpacing(16);
    routingGrid->setVerticalSpacing(12);
    routingGrid->setColumnStretch(1, 1);
    routingGrid->setColumnStretch(3, 1);

    m_defaultOutboundCombo = new MenuComboBox;
    m_defaultOutboundCombo->addItems({tr("Manual selector (recommended)"), tr("Auto select (URLTest)")});
    m_defaultOutboundCombo->setWheelEnabled(false);
    m_defaultOutboundCombo->setFixedHeight(kSpinBoxHeight);
    m_defaultOutboundCombo->setMinimumWidth(150);

    m_downloadDetourCombo = new MenuComboBox;
    m_downloadDetourCombo->addItems({tr("Manual selector"), tr("Direct")});
    m_downloadDetourCombo->setWheelEnabled(false);
    m_downloadDetourCombo->setFixedHeight(kSpinBoxHeight);
    m_downloadDetourCombo->setMinimumWidth(150);

    QLabel *defaultOutboundLabel = createFormLabel(tr("Default outbound for non-CN traffic"));
    QLabel *downloadDetourLabel = createFormLabel(tr("Rule-set/UI download detour"));
    matchLabelWidth(defaultOutboundLabel, downloadDetourLabel);

    routingGrid->addWidget(defaultOutboundLabel, 0, 0);
    routingGrid->addWidget(m_defaultOutboundCombo, 0, 1);
    routingGrid->addWidget(downloadDetourLabel, 0, 2);
    routingGrid->addWidget(m_downloadDetourCombo, 0, 3);

    singboxProfileCardLayout->addLayout(routingGrid);

    QWidget *profileToggleCard = new QWidget;
    profileToggleCard->setObjectName("SettingsToggleCard");
    QHBoxLayout *profileToggleLayout = new QHBoxLayout(profileToggleCard);
    profileToggleLayout->setContentsMargins(16, 10, 16, 10);
    profileToggleLayout->setSpacing(30);

    auto addProfileToggle = [profileToggleLayout](const QString &text, ToggleSwitch *&toggle) {
        QWidget *item = new QWidget;
        QHBoxLayout *itemLayout = new QHBoxLayout(item);
        itemLayout->setContentsMargins(0, 0, 0, 0);
        itemLayout->setSpacing(10);
        QLabel *label = new QLabel(text);
        toggle = new ToggleSwitch;
        itemLayout->addWidget(label);
        itemLayout->addWidget(toggle);
        itemLayout->addStretch();
        profileToggleLayout->addWidget(item);
    };

    addProfileToggle(tr("Block ads (geosite-category-ads-all)"), m_blockAdsSwitch);
    addProfileToggle(tr("DNS hijack (hijack-dns)"), m_dnsHijackSwitch);
    addProfileToggle(tr("Enable app groups (TG/YouTube/Netflix/OpenAI)"), m_enableAppGroupsSwitch);
    profileToggleLayout->addStretch();
    singboxProfileCardLayout->addWidget(profileToggleCard);

    QLabel *dnsTitle = new QLabel(tr("DNS"));
    dnsTitle->setProperty("class", "SettingsSectionSubTitle");
    singboxProfileCardLayout->addWidget(dnsTitle);

    QGridLayout *dnsGrid = new QGridLayout;
    dnsGrid->setHorizontalSpacing(16);
    dnsGrid->setVerticalSpacing(12);
    dnsGrid->setColumnStretch(1, 1);
    dnsGrid->setColumnStretch(3, 1);

    m_dnsProxyEdit = new QLineEdit;
    m_dnsProxyEdit->setPlaceholderText(ConfigConstants::DEFAULT_DNS_PROXY);
    m_dnsProxyEdit->setFixedHeight(kSpinBoxHeight);
    m_dnsProxyEdit->setMinimumWidth(150);

    m_dnsCnEdit = new QLineEdit;
    m_dnsCnEdit->setPlaceholderText(ConfigConstants::DEFAULT_DNS_CN);
    m_dnsCnEdit->setFixedHeight(kSpinBoxHeight);
    m_dnsCnEdit->setMinimumWidth(150);

    m_dnsResolverEdit = new QLineEdit;
    m_dnsResolverEdit->setPlaceholderText(ConfigConstants::DEFAULT_DNS_RESOLVER);
    m_dnsResolverEdit->setFixedHeight(kSpinBoxHeight);
    m_dnsResolverEdit->setMinimumWidth(150);

    m_urltestUrlEdit = new QLineEdit;
    m_urltestUrlEdit->setPlaceholderText(ConfigConstants::DEFAULT_URLTEST_URL);
    m_urltestUrlEdit->setFixedHeight(kSpinBoxHeight);
    m_urltestUrlEdit->setMinimumWidth(150);

    QLabel *dnsProxyLabel = createFormLabel(tr("Proxy DNS (non-CN)"));
    QLabel *dnsCnLabel = createFormLabel(tr("CN DNS"));
    matchLabelWidth(dnsProxyLabel, dnsCnLabel);

    QLabel *dnsResolverLabel = createFormLabel(tr("Resolver DNS (for DoH hostname resolving)"));
    QLabel *urltestLabel = createFormLabel(tr("URLTest URL"));
    matchLabelWidth(dnsResolverLabel, urltestLabel);

    dnsGrid->addWidget(dnsProxyLabel, 0, 0);
    dnsGrid->addWidget(m_dnsProxyEdit, 0, 1);
    dnsGrid->addWidget(dnsCnLabel, 0, 2);
    dnsGrid->addWidget(m_dnsCnEdit, 0, 3);
    dnsGrid->addWidget(dnsResolverLabel, 1, 0);
    dnsGrid->addWidget(m_dnsResolverEdit, 1, 1);
    dnsGrid->addWidget(urltestLabel, 1, 2);
    dnsGrid->addWidget(m_urltestUrlEdit, 1, 3);

    singboxProfileCardLayout->addLayout(dnsGrid);

    singboxProfileLayout->addWidget(singboxProfileCard);
    return singboxProfileSection;
}

QWidget* SettingsView::buildAppearanceSection()
{
    QWidget *appearanceSection = new QWidget;
    QVBoxLayout *appearanceSectionLayout = new QVBoxLayout(appearanceSection);
    appearanceSectionLayout->setContentsMargins(0, 0, 0, 0);
    appearanceSectionLayout->setSpacing(12);
    appearanceSectionLayout->addWidget(createSectionTitle(tr("Appearance")));

    QFrame *appearanceCard = createCard();
    QGridLayout *appearanceLayout = new QGridLayout(appearanceCard);
    appearanceLayout->setContentsMargins(20, 20, 20, 20);
    appearanceLayout->setHorizontalSpacing(16);
    appearanceLayout->setVerticalSpacing(12);
    appearanceLayout->setColumnStretch(1, 1);
    appearanceLayout->setColumnStretch(3, 1);

    QLabel *themeLabel = createFormLabel(tr("Theme:"));
    QLabel *languageLabel = createFormLabel(tr("Language:"));
    matchLabelWidth(themeLabel, languageLabel);

    m_themeCombo = new MenuComboBox;
    m_themeCombo->addItems({tr("Dark"), tr("Light"), tr("Follow System")});
    m_themeCombo->setWheelEnabled(false);
    m_themeCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_themeCombo->setFixedHeight(kSpinBoxHeight);
    m_themeCombo->setMinimumWidth(150);

    m_languageCombo = new MenuComboBox;
    m_languageCombo->addItems({tr("Simplified Chinese"), "English", tr("Japanese"), tr("Russian")});
    m_languageCombo->setWheelEnabled(false);
    m_languageCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_languageCombo->setFixedHeight(kSpinBoxHeight);
    m_languageCombo->setMinimumWidth(150);

    appearanceLayout->addWidget(themeLabel, 0, 0);
    appearanceLayout->addWidget(m_themeCombo, 0, 1);
    appearanceLayout->addWidget(languageLabel, 0, 2);
    appearanceLayout->addWidget(m_languageCombo, 0, 3);

    appearanceSectionLayout->addWidget(appearanceCard);
    return appearanceSection;
}

QWidget* SettingsView::buildKernelSection()
{
    ThemeService *ts = ThemeProvider::instance();
    Q_UNUSED(ts); // Colors obtained via QSS

    QWidget *kernelSection = new QWidget;
    QVBoxLayout *kernelSectionLayout = new QVBoxLayout(kernelSection);
    kernelSectionLayout->setContentsMargins(0, 0, 0, 0);
    kernelSectionLayout->setSpacing(12);
    kernelSectionLayout->addWidget(createSectionTitle(tr("Kernel Settings")));

    QFrame *kernelCard = createCard();
    QFormLayout *kernelLayout = new QFormLayout(kernelCard);
    kernelLayout->setContentsMargins(20, 20, 20, 20);
    kernelLayout->setSpacing(15);
    kernelLayout->setLabelAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    m_kernelVersionLabel = new QLabel(tr("Not installed"));
    m_kernelVersionLabel->setObjectName("KernelVersionLabel");
    m_kernelVersionLabel->setProperty("status", "error");

    m_kernelVersionCombo = new MenuComboBox;
    m_kernelVersionCombo->addItem(tr("Latest version"));
    m_kernelVersionCombo->setWheelEnabled(false);
    m_kernelVersionCombo->setFixedHeight(kSpinBoxHeight);
    m_kernelVersionCombo->setMinimumWidth(150);

    m_kernelPathEdit = new QLineEdit;
    m_kernelPathEdit->setReadOnly(true);
    m_kernelPathEdit->setPlaceholderText(tr("Kernel path"));
    m_kernelPathEdit->setFixedHeight(kSpinBoxHeight);
    m_kernelPathEdit->setMinimumWidth(150);

    m_kernelDownloadProgress = new QProgressBar;
    m_kernelDownloadProgress->setObjectName("KernelProgress");
    m_kernelDownloadProgress->setRange(0, 100);
    m_kernelDownloadProgress->setValue(0);
    m_kernelDownloadProgress->setTextVisible(true);
    m_kernelDownloadProgress->setVisible(false);

    m_kernelDownloadStatus = new QLabel;
    m_kernelDownloadStatus->setObjectName("KernelStatusLabel");
    m_kernelDownloadStatus->setVisible(false);

    QHBoxLayout *kernelBtnLayout = new QHBoxLayout;

    m_downloadKernelBtn = new QPushButton(tr("Download Kernel"));
    m_downloadKernelBtn->setObjectName("DownloadKernelBtn");
    m_checkKernelBtn = new QPushButton(tr("Check Installation"));
    m_checkKernelBtn->setObjectName("CheckKernelBtn");
    m_checkUpdateBtn = new QPushButton(tr("Check Updates"));
    m_checkUpdateBtn->setObjectName("CheckUpdateBtn");

    kernelBtnLayout->addWidget(m_downloadKernelBtn);
    kernelBtnLayout->addWidget(m_checkKernelBtn);
    kernelBtnLayout->addWidget(m_checkUpdateBtn);
    kernelBtnLayout->addStretch();

    kernelLayout->addRow(createFormLabel(tr("Installed version:")), m_kernelVersionLabel);
    kernelLayout->addRow(createFormLabel(tr("Select version:")), m_kernelVersionCombo);
    kernelLayout->addRow(createFormLabel(tr("Kernel path:")), m_kernelPathEdit);
    kernelLayout->addRow(m_kernelDownloadProgress);
    kernelLayout->addRow(m_kernelDownloadStatus);
    kernelLayout->addRow(kernelBtnLayout);

    kernelSectionLayout->addWidget(kernelCard);
    return kernelSection;
}

void SettingsView::setupUI()
{
    ThemeService *ts = ThemeProvider::instance();
    Q_UNUSED(ts); // Colors obtained via QSS

    // 使用全局 QSS，不再单独拼接控件样式。
    m_inputStyleApplied.clear();
    m_comboStyle.clear();

    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setObjectName("SettingsScroll");

    QWidget *contentWidget = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(contentWidget);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    // Header (match Rules page layout spacing)
    QHBoxLayout *headerLayout = new QHBoxLayout;
    QVBoxLayout *titleLayout = new QVBoxLayout;
    titleLayout->setSpacing(4);

    QLabel *titleLabel = new QLabel(tr("Settings"));
    titleLabel->setObjectName("PageTitle");
    QLabel *subtitleLabel = new QLabel(tr("Configure application preferences"));
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
    m_saveBtn->setFixedHeight(36);
    m_saveBtn->setFixedWidth(110);
    mainLayout->addWidget(m_saveBtn, 0, Qt::AlignHCenter);

    scrollArea->setWidget(contentWidget);
    outerLayout->addWidget(scrollArea, 1);

    connect(m_saveBtn, &QPushButton::clicked, this, &SettingsView::onSaveClicked);
    connect(m_downloadKernelBtn, &QPushButton::clicked, this, &SettingsView::onDownloadKernelClicked);
    connect(m_checkKernelBtn, &QPushButton::clicked, this, &SettingsView::onCheckKernelClicked);
    connect(m_checkUpdateBtn, &QPushButton::clicked, this, &SettingsView::onCheckUpdateClicked);

    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        ThemeManager::instance().setThemeMode(SettingsHelpers::themeModeFromIndex(index));
    });
    connect(m_languageCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (index == kLanguageDefaultIndex) {
            return;
        }
        QMessageBox::information(this, tr("提示"), tr("正在适配中"));
        QSignalBlocker blocker(m_languageCombo);
        m_languageCombo->setCurrentIndex(kLanguageDefaultIndex);
    });
}

void SettingsView::updateStyle()
{
    ThemeService *ts = ThemeProvider::instance();
    if (!ts) return;
    setStyleSheet(ts->loadStyleSheet(":/styles/settings_view.qss"));
    {
        QSignalBlocker blocker(m_themeCombo);
        if (m_themeCombo) {
            m_themeCombo->setCurrentIndex(SettingsHelpers::themeIndexFromMode(ThemeManager::instance().getThemeMode()));
        }
    }
}

QString SettingsView::normalizeBypassText(const QString &text) const
{
    return SettingsHelpers::normalizeBypassText(text);
}

void SettingsView::fillGeneralFromUi(SettingsModel::Data &data) const
{
    data.mixedPort = m_mixedPortSpin->value();
    data.apiPort = m_apiPortSpin->value();
    data.autoStart = m_autoStartCheck->isChecked();
    data.systemProxyEnabled = m_systemProxyCheck->isChecked();
}

void SettingsView::fillAdvancedFromUi(SettingsModel::Data &data) const
{
    data.systemProxyBypass = normalizeBypassText(m_systemProxyBypassEdit->toPlainText());
    data.tunMtu = m_tunMtuSpin->value();
    switch (m_tunStackCombo->currentIndex()) {
        case 1: data.tunStack = "system"; break;
        case 2: data.tunStack = "gvisor"; break;
        default: data.tunStack = "mixed"; break;
    }
    data.tunEnableIpv6 = m_tunEnableIpv6Switch->isChecked();
    data.tunAutoRoute = m_tunAutoRouteSwitch->isChecked();
    data.tunStrictRoute = m_tunStrictRouteSwitch->isChecked();
}

void SettingsView::fillProfileFromUi(SettingsModel::Data &data) const
{
    data.defaultOutbound = (m_defaultOutboundCombo->currentIndex() == 1) ? "auto" : "manual";
    data.downloadDetour = (m_downloadDetourCombo->currentIndex() == 0) ? "manual" : "direct";
    data.blockAds = m_blockAdsSwitch->isChecked();
    data.dnsHijack = m_dnsHijackSwitch->isChecked();
    data.enableAppGroups = m_enableAppGroupsSwitch->isChecked();

    data.dnsProxy = SettingsHelpers::resolveTextOrDefault(m_dnsProxyEdit, ConfigConstants::DEFAULT_DNS_PROXY);
    data.dnsCn = SettingsHelpers::resolveTextOrDefault(m_dnsCnEdit, ConfigConstants::DEFAULT_DNS_CN);
    data.dnsResolver = SettingsHelpers::resolveTextOrDefault(m_dnsResolverEdit, ConfigConstants::DEFAULT_DNS_RESOLVER);
    data.urltestUrl = SettingsHelpers::resolveTextOrDefault(m_urltestUrlEdit, ConfigConstants::DEFAULT_URLTEST_URL);
}

void SettingsView::applySettingsToUi(const SettingsModel::Data &data)
{
    m_mixedPortSpin->setValue(data.mixedPort);
    m_apiPortSpin->setValue(data.apiPort);

    m_autoStartCheck->setChecked(data.autoStart);

    m_systemProxyCheck->setChecked(data.systemProxyEnabled);
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

    m_defaultOutboundCombo->setCurrentIndex(data.defaultOutbound == "auto" ? 1 : 0);
    m_downloadDetourCombo->setCurrentIndex(data.downloadDetour == "manual" ? 0 : 1);
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
            m_themeCombo->setCurrentIndex(SettingsHelpers::themeIndexFromMode(ThemeManager::instance().getThemeMode()));
        }
    }
    {
        QSignalBlocker blocker(m_languageCombo);
        m_languageCombo->setCurrentIndex(kLanguageDefaultIndex);
    }
}

void SettingsView::loadSettings()
{
    applySettingsToUi(SettingsService::loadSettings());
}

bool SettingsView::saveSettings()
{
    SettingsModel::Data data = SettingsService::loadSettings();
    fillGeneralFromUi(data);
    fillAdvancedFromUi(data);
    fillProfileFromUi(data);

    QString errorMessage;
    if (!SettingsService::saveSettings(data,
                                       m_themeCombo->currentIndex(),
                                       m_languageCombo->currentIndex(),
                                       &errorMessage)) {
        QMessageBox::warning(this, tr("Notice"), errorMessage);
        return false;
    }
    return true;
}

void SettingsView::onSaveClicked()
{
    if (saveSettings()) {
        QMessageBox::information(this, tr("Notice"), tr("Settings saved"));
    }
}

void SettingsView::onDownloadKernelClicked()
{
    if (m_isDownloading || !m_kernelManager) {
        return;
    }
    QString version;
    if (m_kernelVersionCombo && m_kernelVersionCombo->currentIndex() > 0) {
        version = m_kernelVersionCombo->currentText().trimmed();
    }
    setDownloadUi(true, tr("Preparing to download kernel..."));
    m_kernelManager->downloadAndInstall(version);
}

void SettingsView::onCheckKernelClicked()
{
    if (m_kernelManager) {
        m_checkingInstall = true;
        setDownloadUi(true, tr("Checking installation..."));
        m_kernelManager->refreshInstalledInfo();
        m_kernelManager->fetchReleaseList();
    } else {
        setDownloadUi(false, tr("Kernel manager unavailable"));
    }
}

void SettingsView::onCheckUpdateClicked()
{
    if (m_kernelManager) {
        setDownloadUi(true, tr("Checking latest kernel version..."));
        m_kernelManager->checkLatest();
    }
}

void SettingsView::onKernelInstalledReady(const QString &path, const QString &version)
{
    setDownloadUi(false, tr("Installation check finished"));
    if (m_kernelPathEdit) {
        m_kernelPathEdit->setText(path);
    }
    if (m_kernelVersionLabel) {
        if (version.isEmpty()) {
            m_kernelVersionLabel->setText(tr("Not installed"));
            m_kernelVersionLabel->setProperty("status", "error");
        } else {
            m_kernelVersionLabel->setText(version);
            m_kernelVersionLabel->setProperty("status", "success");
        }
    }
    if (m_checkingInstall) {
        m_checkingInstall = false;
        const QString msg = version.isEmpty()
            ? tr("Kernel is not installed.")
            : tr("Kernel installed. Version: %1\nPath: %2").arg(version, path.isEmpty() ? tr("Unknown") : path);
        QMessageBox::information(this, tr("Check Installation"), msg);
    }
}

void SettingsView::onKernelReleasesReady(const QStringList &versions, const QString &latest)
{
    setDownloadUi(false);
    if (!m_kernelVersionCombo) return;
    m_kernelVersionCombo->clear();
    m_kernelVersionCombo->addItem(tr("Latest version"));
    for (const QString &ver : versions) {
        m_kernelVersionCombo->addItem(ver);
    }
}

void SettingsView::onKernelLatestReady(const QString &latest, const QString &installed)
{
    setDownloadUi(false);
    if (installed.isEmpty()) {
        QMessageBox::information(this, tr("Check Updates"),
            tr("Kernel not installed. Latest version is %1").arg(latest));
        return;
    }
    if (installed == latest) {
        QMessageBox::information(this, tr("Check Updates"), tr("Already on the latest version"));
    } else {
        QMessageBox::information(this, tr("Check Updates"),
            tr("New kernel version %1 available, current %2").arg(latest, installed));
    }
}

void SettingsView::onKernelDownloadProgress(int percent)
{
    if (m_kernelDownloadProgress) {
        m_kernelDownloadProgress->setValue(percent);
        m_kernelDownloadProgress->setVisible(true);
    }
}

void SettingsView::onKernelStatusChanged(const QString &status)
{
    setDownloadUi(true, status);
}

void SettingsView::onKernelFinished(bool ok, const QString &message)
{
    setDownloadUi(false, message);
    if (ok) {
        QMessageBox::information(this, tr("Kernel"), message);
    } else {
        QMessageBox::warning(this, tr("Kernel"), message);
    }
}

void SettingsView::setDownloadUi(bool downloading, const QString &message)
{
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
