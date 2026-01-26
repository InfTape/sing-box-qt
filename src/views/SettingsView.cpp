#include "SettingsView.h"
#include "storage/DatabaseService.h"
#include "storage/AppSettings.h"
#include "storage/ConfigConstants.h"
#include "system/UpdateService.h"
#include "system/AutoStart.h"
#include "utils/Logger.h"
#include "network/HttpClient.h"
#include "core/ProcessManager.h"
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
#include <QStandardPaths>
#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSysInfo>
#include <QJsonObject>
#include <QOperatingSystemVersion>
#include <QWheelEvent>
#include <QSignalBlocker>
#include <algorithm>
#include <functional>
#include "utils/AppPaths.h"
#include "utils/ThemeManager.h"
#include "widgets/MenuComboBox.h"
#include "widgets/ToggleSwitch.h"

namespace {

constexpr int kThemeDefaultIndex = 0;
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


QString normalizeVersionTag(const QString &raw)
{
    QString ver = raw.trimmed();
    if (ver.startsWith('v')) {
        ver = ver.mid(1);
    }
    return ver;
}

bool isPreReleaseTag(const QString &tag)
{
    const QString lower = tag.toLower();
    return lower.contains("rc") || lower.contains("beta") || lower.contains("alpha");
}

QStringList latestKernelApiUrls()
{
    return {
        "https://api.github.com/repos/SagerNet/sing-box/releases/latest",
        "https://v6.gh-proxy.com/https://api.github.com/repos/SagerNet/sing-box/releases/latest",
        "https://gh-proxy.com/https://api.github.com/repos/SagerNet/sing-box/releases/latest",
        "https://ghfast.top/https://api.github.com/repos/SagerNet/sing-box/releases/latest",
    };
}

QStringList kernelReleasesApiUrls()
{
    return {
        "https://api.github.com/repos/SagerNet/sing-box/releases",
        "https://v6.gh-proxy.com/https://api.github.com/repos/SagerNet/sing-box/releases",
        "https://gh-proxy.com/https://api.github.com/repos/SagerNet/sing-box/releases",
        "https://ghfast.top/https://api.github.com/repos/SagerNet/sing-box/releases",
    };
}

QString kernelInstallDir()
{
    return appDataDir();
}
} // namespace

SettingsView::SettingsView(QWidget *parent)
    : QWidget(parent)
    , m_httpClient(new HttpClient(this))
{
    setupUI();
    loadSettings();
    refreshKernelInfo();
    fetchKernelVersions();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &SettingsView::updateStyle);
    updateStyle();
}

void SettingsView::setupUI()
{
    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setObjectName("SettingsScroll");
    scrollArea->setStyleSheet("QScrollArea { background: transparent; } QScrollArea > QWidget > QWidget { background: transparent; }");

    QWidget *contentWidget = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(contentWidget);
    mainLayout->setContentsMargins(30, 30, 30, 30);
    mainLayout->setSpacing(20);

    ThemeManager &tm = ThemeManager::instance();
    QString inputStyle = R"(
        QSpinBox, QLineEdit, QPlainTextEdit {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 10px;
            padding: 8px 12px;
            color: #eaeaea;
            min-width: 150px;
        }
        QPlainTextEdit {
            min-height: 80px;
        }
        QSpinBox::up-button, QSpinBox::down-button {
            width: 0px;
            height: 0px;
            border: none;
            margin: 0px;
            padding: 0px;
        }
        QSpinBox::up-arrow, QSpinBox::down-arrow {
            image: none;
        }
        QCheckBox {
            color: #eaeaea;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border-radius: 4px;
            border: 1px solid %2;
            background-color: %1;
        }
        QCheckBox::indicator:checked {
            background-color: #00aaff;
            border-color: #00aaff;
            image: url(:/icons/check.svg);
        }
    )";
    const QString inputStyleApplied = inputStyle.arg(tm.getColorString("bg-primary"),
                                                     tm.getColorString("border"));

    auto makeSectionTitle = [&tm](const QString &text) {
        QLabel *title = new QLabel(text);
        title->setStyleSheet(QString("font-size: 13px; font-weight: 600; color: %1;")
            .arg(tm.getColorString("text-tertiary")));
        return title;
    };

    auto makeCard = [&tm]() {
        QFrame *card = new QFrame;
        card->setObjectName("SettingsCard");
        card->setStyleSheet(QString("QFrame#SettingsCard { background-color: %1; border: none; border-radius: 10px; }")
            .arg(tm.getColorString("panel-bg")));
        return card;
    };
    auto makeFormLabel = [](const QString &text) {
        QLabel *label = new QLabel(text);
        label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        label->setStyleSheet("padding-top: 3px;");
        return label;
    };
    auto matchLabelWidth = [](QLabel *left, QLabel *right) {
        if (!left || !right) {
            return;
        }
        const int width = std::max(left->sizeHint().width(), right->sizeHint().width());
        left->setFixedWidth(width);
        right->setFixedWidth(width);
    };


    QWidget *proxySection = new QWidget;
    QVBoxLayout *proxySectionLayout = new QVBoxLayout(proxySection);
    proxySectionLayout->setContentsMargins(0, 0, 0, 0);
    proxySectionLayout->setSpacing(12);

    proxySectionLayout->addWidget(makeSectionTitle(tr("Proxy Settings")));

    QFrame *proxyCard = makeCard();

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
    m_mixedPortSpin->setStyleSheet(inputStyleApplied);
    m_mixedPortSpin->setFixedHeight(kSpinBoxHeight);
    m_mixedPortSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_apiPortSpin = new QSpinBox;
    m_apiPortSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_apiPortSpin->setRange(1, 65535);
    m_apiPortSpin->setValue(9090);
    m_apiPortSpin->setStyleSheet(inputStyleApplied);
    m_apiPortSpin->setFixedHeight(kSpinBoxHeight);
    m_apiPortSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_autoStartCheck = new QCheckBox(tr("Auto start on boot"));
    m_autoStartCheck->setStyleSheet(inputStyleApplied);

    m_systemProxyCheck = new QCheckBox(tr("Auto-set system proxy"));
    m_systemProxyCheck->setStyleSheet(inputStyleApplied);

    QLabel *mixedPortLabel = makeFormLabel(tr("Mixed port:"));
    QLabel *apiPortLabel = makeFormLabel(tr("API port:"));
    matchLabelWidth(mixedPortLabel, apiPortLabel);

    proxyLayout->addWidget(mixedPortLabel, 0, 0);
    proxyLayout->addWidget(m_mixedPortSpin, 0, 1);
    proxyLayout->addWidget(apiPortLabel, 0, 2);
    proxyLayout->addWidget(m_apiPortSpin, 0, 3);
    proxyLayout->addWidget(m_autoStartCheck, 1, 0, 1, 4);
    proxyLayout->addWidget(m_systemProxyCheck, 2, 0, 1, 4);

    proxySectionLayout->addWidget(proxyCard);

    QWidget *proxyAdvancedSection = new QWidget;
    QVBoxLayout *proxyAdvancedLayout = new QVBoxLayout(proxyAdvancedSection);
    proxyAdvancedLayout->setContentsMargins(0, 0, 0, 0);
    proxyAdvancedLayout->setSpacing(12);
    proxyAdvancedLayout->addWidget(makeSectionTitle(tr("Proxy Advanced Settings")));

    QFrame *proxyAdvancedCard = makeCard();
    QVBoxLayout *advancedLayout = new QVBoxLayout(proxyAdvancedCard);
    advancedLayout->setContentsMargins(20, 20, 20, 20);
    advancedLayout->setSpacing(16);

    QLabel *bypassLabel = new QLabel(tr("System proxy bypass domains"));
    bypassLabel->setStyleSheet("color: #cbd5e1;");
    m_systemProxyBypassEdit = new QPlainTextEdit;
    m_systemProxyBypassEdit->setPlaceholderText(ConfigConstants::DEFAULT_SYSTEM_PROXY_BYPASS);
    m_systemProxyBypassEdit->setStyleSheet(inputStyleApplied);

    advancedLayout->addWidget(bypassLabel);
    advancedLayout->addWidget(m_systemProxyBypassEdit);

    QLabel *tunTitle = new QLabel(tr("TUN Virtual Adapter"));
    tunTitle->setStyleSheet("color: #cbd5e1; font-weight: bold;");
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
    m_tunMtuSpin->setStyleSheet(inputStyleApplied);
    m_tunMtuSpin->setFixedHeight(kSpinBoxHeight);
    m_tunMtuSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_tunStackCombo = new MenuComboBox;
    m_tunStackCombo->addItems({tr("Mixed"), tr("System"), tr("gVisor")});
    m_tunStackCombo->setWheelEnabled(false);
    m_tunStackCombo->setFixedHeight(kSpinBoxHeight);
    m_tunStackCombo->setStyleSheet(QString(R"(
        QComboBox {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 10px;
            padding: 6px 12px;
            color: #eaeaea;
            min-width: 150px;
        }
    )").arg(tm.getColorString("bg-primary"),
            tm.getColorString("border")));

    QLabel *mtuLabel = makeFormLabel(tr("MTU:"));
    QLabel *stackLabel = makeFormLabel(tr("Protocol stack:"));
    matchLabelWidth(mtuLabel, stackLabel);

    tunLeft->addRow(mtuLabel, m_tunMtuSpin);
    tunRight->addRow(stackLabel, m_tunStackCombo);
    tunRow->addLayout(tunLeft, 1);
    tunRow->addLayout(tunRight, 1);
    advancedLayout->addLayout(tunRow);

    QWidget *toggleCard = new QWidget;
    toggleCard->setStyleSheet(QString("background-color: %1; border-radius: 12px;")
        .arg(tm.getColorString("bg-secondary")));
    QHBoxLayout *toggleLayout = new QHBoxLayout(toggleCard);
    toggleLayout->setContentsMargins(16, 10, 16, 10);
    toggleLayout->setSpacing(30);

    auto addToggle = [toggleLayout](const QString &text, ToggleSwitch *&toggle) {
        QWidget *item = new QWidget;
        QHBoxLayout *itemLayout = new QHBoxLayout(item);
        itemLayout->setContentsMargins(0, 0, 0, 0);
        itemLayout->setSpacing(10);
        QLabel *label = new QLabel(text);
        label->setStyleSheet("color: #eaeaea;");
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
    advancedHint->setStyleSheet("color: #94a3b8; font-size: 12px;");
    advancedLayout->addWidget(advancedHint);

    m_saveAdvancedBtn = new QPushButton(tr("Save Advanced Settings"));
    advancedLayout->addWidget(m_saveAdvancedBtn);

    proxyAdvancedLayout->addWidget(proxyAdvancedCard);

    QWidget *appearanceSection = new QWidget;
    QVBoxLayout *appearanceSectionLayout = new QVBoxLayout(appearanceSection);
    appearanceSectionLayout->setContentsMargins(0, 0, 0, 0);
    appearanceSectionLayout->setSpacing(12);
    appearanceSectionLayout->addWidget(makeSectionTitle(tr("Appearance")));

    QFrame *appearanceCard = makeCard();
    QGridLayout *appearanceLayout = new QGridLayout(appearanceCard);
    appearanceLayout->setContentsMargins(20, 20, 20, 20);
    appearanceLayout->setHorizontalSpacing(16);
    appearanceLayout->setVerticalSpacing(12);
    appearanceLayout->setColumnStretch(1, 1);
    appearanceLayout->setColumnStretch(3, 1);

    QLabel *themeLabel = makeFormLabel(tr("Theme:"));
    QLabel *languageLabel = makeFormLabel(tr("Language:"));
    matchLabelWidth(themeLabel, languageLabel);

    m_themeCombo = new MenuComboBox;
    m_themeCombo->addItems({tr("Dark"), tr("Light"), tr("Follow System")});
    m_themeCombo->setWheelEnabled(false);
    m_themeCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_themeCombo->setFixedHeight(kSpinBoxHeight);

    m_languageCombo = new MenuComboBox;
    m_languageCombo->addItems({tr("Simplified Chinese"), "English", tr("Japanese"), tr("Russian")});
    m_languageCombo->setWheelEnabled(false);
    m_languageCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_languageCombo->setFixedHeight(kSpinBoxHeight);

    appearanceLayout->addWidget(themeLabel, 0, 0);
    appearanceLayout->addWidget(m_themeCombo, 0, 1);
    appearanceLayout->addWidget(languageLabel, 0, 2);
    appearanceLayout->addWidget(m_languageCombo, 0, 3);

    appearanceSectionLayout->addWidget(appearanceCard);


    QWidget *kernelSection = new QWidget;
    QVBoxLayout *kernelSectionLayout = new QVBoxLayout(kernelSection);
    kernelSectionLayout->setContentsMargins(0, 0, 0, 0);
    kernelSectionLayout->setSpacing(12);
    kernelSectionLayout->addWidget(makeSectionTitle(tr("Kernel Settings")));

    QFrame *kernelCard = makeCard();
    QFormLayout *kernelLayout = new QFormLayout(kernelCard);
    kernelLayout->setContentsMargins(20, 20, 20, 20);
    kernelLayout->setSpacing(15);
    kernelLayout->setLabelAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    m_kernelVersionLabel = new QLabel(tr("Not installed"));
    m_kernelVersionLabel->setStyleSheet("color: #e94560; font-weight: bold;");

    m_kernelVersionCombo = new MenuComboBox;
    m_kernelVersionCombo->addItem(tr("Latest version"));
    m_kernelVersionCombo->setWheelEnabled(false);
    m_kernelVersionCombo->setFixedHeight(kSpinBoxHeight);

    m_kernelPathEdit = new QLineEdit;
    m_kernelPathEdit->setReadOnly(true);
    m_kernelPathEdit->setPlaceholderText(tr("Kernel path"));
    m_kernelPathEdit->setStyleSheet(inputStyleApplied);
    m_kernelPathEdit->setFixedHeight(kSpinBoxHeight);

    m_kernelDownloadProgress = new QProgressBar;
    m_kernelDownloadProgress->setRange(0, 100);
    m_kernelDownloadProgress->setValue(0);
    m_kernelDownloadProgress->setTextVisible(true);
    m_kernelDownloadProgress->setVisible(false);
    m_kernelDownloadProgress->setStyleSheet(R"(
        QProgressBar {
            background-color: #0f3460;
            border: none;
            border-radius: 8px;
            color: #eaeaea;
            height: 16px;
        }
        QProgressBar::chunk {
            background-color: #4ecca3;
            border-radius: 8px;
        }
    )");

    m_kernelDownloadStatus = new QLabel;
    m_kernelDownloadStatus->setStyleSheet("color: #cbd5e1; font-size: 12px;");
    m_kernelDownloadStatus->setVisible(false);

    QHBoxLayout *kernelBtnLayout = new QHBoxLayout;

    m_downloadKernelBtn = new QPushButton(tr("Download Kernel"));

    m_checkKernelBtn = new QPushButton(tr("Check Installation"));

    m_checkUpdateBtn = new QPushButton(tr("Check Updates"));

    kernelBtnLayout->addWidget(m_downloadKernelBtn);
    kernelBtnLayout->addWidget(m_checkKernelBtn);
    kernelBtnLayout->addWidget(m_checkUpdateBtn);
    kernelBtnLayout->addStretch();

    kernelLayout->addRow(makeFormLabel(tr("Installed version:")), m_kernelVersionLabel);
    kernelLayout->addRow(makeFormLabel(tr("Select version:")), m_kernelVersionCombo);
    kernelLayout->addRow(makeFormLabel(tr("Kernel path:")), m_kernelPathEdit);
    kernelLayout->addRow(m_kernelDownloadProgress);
    kernelLayout->addRow(m_kernelDownloadStatus);
    kernelLayout->addRow(kernelBtnLayout);

    kernelSectionLayout->addWidget(kernelCard);

    m_saveBtn = new QPushButton(tr("Save"));
    m_saveBtn->setFixedHeight(36);
    m_saveBtn->setFixedWidth(110);

    mainLayout->addWidget(proxySection);
    mainLayout->addWidget(proxyAdvancedSection);
    mainLayout->addWidget(appearanceSection);
    mainLayout->addWidget(kernelSection);
    mainLayout->addStretch();
    mainLayout->addWidget(m_saveBtn, 0, Qt::AlignHCenter);

    scrollArea->setWidget(contentWidget);
    outerLayout->addWidget(scrollArea, 1);

    connect(m_saveBtn, &QPushButton::clicked, this, &SettingsView::onSaveClicked);
    connect(m_saveAdvancedBtn, &QPushButton::clicked, this, &SettingsView::onSaveAdvancedClicked);
    connect(m_downloadKernelBtn, &QPushButton::clicked, this, &SettingsView::onDownloadKernelClicked);
    connect(m_checkKernelBtn, &QPushButton::clicked, this, &SettingsView::onCheckKernelClicked);
    connect(m_checkUpdateBtn, &QPushButton::clicked, this, &SettingsView::onCheckUpdateClicked);

    connect(m_themeCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (index == kThemeDefaultIndex) {
            return;
        }
        QMessageBox::information(this, tr("提示"), tr("正在适配中"));
        QSignalBlocker blocker(m_themeCombo);
        m_themeCombo->setCurrentIndex(kThemeDefaultIndex);
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
    auto applyTransparentStyle = [](QPushButton *btn, const QColor &baseColor) {
        if (!btn) return;
        QColor bg = baseColor;
        bg.setAlphaF(0.2);
        QColor border = baseColor;
        border.setAlphaF(0.4);
        QColor hoverBg = baseColor;
        hoverBg.setAlphaF(0.3);

        const QString style = QString(
            "QPushButton { background-color: %1; color: %2; border: 1px solid %3; "
            "border-radius: 10px; padding: 10px 20px; font-weight: bold; }"
            "QPushButton:hover { background-color: %4; }"
        ).arg(bg.name(QColor::HexArgb), baseColor.name(), border.name(QColor::HexArgb), hoverBg.name(QColor::HexArgb));
        btn->setStyleSheet(style);
    };

    applyTransparentStyle(m_downloadKernelBtn, QColor("#e94560"));
    applyTransparentStyle(m_checkKernelBtn, QColor("#3b82f6"));
    applyTransparentStyle(m_checkUpdateBtn, QColor("#3b82f6"));
    applyTransparentStyle(m_saveAdvancedBtn, QColor("#3b82f6"));
    applyTransparentStyle(m_saveBtn, QColor("#10b981"));
}

void SettingsView::loadSettings()
{
    QJsonObject config = DatabaseService::instance().getAppConfig();

    m_mixedPortSpin->setValue(config.value("mixedPort").toInt(7890));
    m_apiPortSpin->setValue(config.value("apiPort").toInt(9090));
    bool autoStartWanted = config.value("autoStart").toBool(false);
    if (AutoStart::isSupported()) {
        if (autoStartWanted != AutoStart::isEnabled()) {
            AutoStart::setEnabled(autoStartWanted);
        }
        m_autoStartCheck->setChecked(AutoStart::isEnabled());
    } else {
        m_autoStartCheck->setChecked(autoStartWanted);
    }
    bool systemProxyEnabled = false;
    if (config.contains("systemProxyEnabled")) {
        systemProxyEnabled = config.value("systemProxyEnabled").toBool(false);
    } else {
        systemProxyEnabled = config.value("systemProxy").toBool(false);
    }
    m_systemProxyCheck->setChecked(systemProxyEnabled);
    m_systemProxyBypassEdit->setPlainText(
        config.value("systemProxyBypass").toString(ConfigConstants::DEFAULT_SYSTEM_PROXY_BYPASS));
    m_tunMtuSpin->setValue(config.value("tunMtu").toInt(ConfigConstants::DEFAULT_TUN_MTU));
    const QString tunStack = config.value("tunStack").toString(ConfigConstants::DEFAULT_TUN_STACK);
    if (tunStack == "system") {
        m_tunStackCombo->setCurrentIndex(1);
    } else if (tunStack == "gvisor") {
        m_tunStackCombo->setCurrentIndex(2);
    } else {
        m_tunStackCombo->setCurrentIndex(0);
    }
    m_tunEnableIpv6Switch->setChecked(config.value("tunEnableIpv6").toBool(false));
    m_tunAutoRouteSwitch->setChecked(config.value("tunAutoRoute").toBool(true));
    m_tunStrictRouteSwitch->setChecked(config.value("tunStrictRoute").toBool(true));

    {
        QSignalBlocker blocker(m_themeCombo);
        m_themeCombo->setCurrentIndex(kThemeDefaultIndex);
    }
    {
        QSignalBlocker blocker(m_languageCombo);
        m_languageCombo->setCurrentIndex(kLanguageDefaultIndex);
    }
}

void SettingsView::saveSettings()
{
    QJsonObject config = DatabaseService::instance().getAppConfig();
    config["mixedPort"] = m_mixedPortSpin->value();
    config["apiPort"] = m_apiPortSpin->value();
    bool autoStartEnabled = m_autoStartCheck->isChecked();
    if (AutoStart::isSupported()) {
        if (!AutoStart::setEnabled(autoStartEnabled)) {
            autoStartEnabled = AutoStart::isEnabled();
            m_autoStartCheck->setChecked(autoStartEnabled);
            QMessageBox::warning(this, tr("Notice"), tr("Failed to set auto-start"));
        }
    }
    config["autoStart"] = autoStartEnabled;
    const bool systemProxyEnabled = m_systemProxyCheck->isChecked();
    config["systemProxyEnabled"] = systemProxyEnabled;
    config["systemProxy"] = systemProxyEnabled;

    DatabaseService::instance().saveAppConfig(config);
    AppSettings::instance().load();

    QJsonObject theme;
    switch (m_themeCombo->currentIndex()) {
        case 1: theme["theme"] = "light"; break;
        case 2: theme["theme"] = "auto"; break;
        default: theme["theme"] = "dark"; break;
    }
    DatabaseService::instance().saveThemeConfig(theme);

    QString locales[] = {"zh_CN", "en", "ja", "ru"};
    DatabaseService::instance().saveLocale(locales[m_languageCombo->currentIndex()]);

    Logger::info(tr("Settings saved"));
}

void SettingsView::onSaveAdvancedClicked()
{
    QString bypass = m_systemProxyBypassEdit->toPlainText();
    bypass.replace(QRegularExpression("[\\r\\n]+"), ";");
    bypass = bypass.trimmed();
    if (bypass.isEmpty()) {
        QMessageBox::warning(this, tr("Notice"), tr("Please enter system proxy bypass domains"));
        return;
    }

    const int mtu = m_tunMtuSpin->value();
    if (mtu < 576 || mtu > 9000) {
        QMessageBox::warning(this, tr("Notice"), tr("MTU must be between 576 and 9000"));
        return;
    }

    QString stack = "mixed";
    switch (m_tunStackCombo->currentIndex()) {
        case 1: stack = "system"; break;
        case 2: stack = "gvisor"; break;
        default: stack = "mixed"; break;
    }

    QJsonObject config = DatabaseService::instance().getAppConfig();
    config["systemProxyBypass"] = bypass;
    config["tunMtu"] = mtu;
    config["tunStack"] = stack;
    config["tunEnableIpv6"] = m_tunEnableIpv6Switch->isChecked();
    config["tunAutoRoute"] = m_tunAutoRouteSwitch->isChecked();
    config["tunStrictRoute"] = m_tunStrictRouteSwitch->isChecked();

    DatabaseService::instance().saveAppConfig(config);
    AppSettings::instance().load();

    Logger::info(tr("Advanced settings saved"));
    QMessageBox::information(this, tr("Notice"), tr("Advanced settings saved"));
}

void SettingsView::onSaveClicked()
{
    saveSettings();
    QMessageBox::information(this, tr("Notice"), tr("Settings saved"));
}

void SettingsView::onDownloadKernelClicked()
{
    if (m_isDownloading) {
        return;
    }
    QString version;
    if (m_kernelVersionCombo && m_kernelVersionCombo->currentIndex() > 0) {
        version = m_kernelVersionCombo->currentText().trimmed();
    }
    startKernelDownload(version);
}

void SettingsView::onCheckKernelClicked()
{
    refreshKernelInfo();
    fetchKernelVersions();

    const QString kernelPath = detectKernelPath();
    if (kernelPath.isEmpty()) {
        QMessageBox::warning(this, tr("Check Installation"), tr("sing-box kernel not found. Download it or set the path manually."));
        return;
    }

    const QString version = queryKernelVersion(kernelPath);
    if (version.isEmpty()) {
        QMessageBox::warning(this, tr("Check Installation"),
                             tr("Found kernel but failed to read version:\n%1").arg(kernelPath));
        return;
    }

    QMessageBox::information(this, tr("Check Installation"),
                             tr("Kernel installed.\nPath: %1\nVersion: %2").arg(kernelPath, version));
}

void SettingsView::onCheckUpdateClicked()
{
    if (!m_httpClient) {
        return;
    }

    const QString kernelPath = detectKernelPath();
    const QString installedVersion = queryKernelVersion(kernelPath);

    const QStringList apiUrls = latestKernelApiUrls();
    auto tryFetch = std::make_shared<std::function<void(int)>>();
    *tryFetch = [this, apiUrls, installedVersion, tryFetch](int index) {
        if (index >= apiUrls.size()) {
            QMessageBox::warning(this, tr("Check Updates"), tr("Failed to fetch kernel versions. Please try again."));
            return;
        }

        const QString url = apiUrls.at(index);
        m_httpClient->get(url, [this, apiUrls, installedVersion, index, tryFetch](bool success, const QByteArray &data) {
            if (!success) {
                (*tryFetch)(index + 1);
                return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isObject()) {
                (*tryFetch)(index + 1);
                return;
            }

            const QJsonObject obj = doc.object();
            QString latest = normalizeVersionTag(obj.value("tag_name").toString());
            QString installed = normalizeVersionTag(installedVersion);

            if (latest.isEmpty()) {
                (*tryFetch)(index + 1);
                return;
            }

            if (installed.isEmpty()) {
                QMessageBox::information(this, tr("Check Updates"),
                    tr("Kernel not installed. Latest version is %1").arg(latest));
                return;
            }

            if (installed == latest) {
                QMessageBox::information(this, tr("Check Updates"), tr("Already on the latest version"));
                return;
            }

            QMessageBox::information(this, tr("Check Updates"),
                tr("New kernel version %1 available, current %2").arg(latest, installed));
        });
    };

    (*tryFetch)(0);
}

void SettingsView::refreshKernelInfo()
{
    const QString kernelPath = detectKernelPath();
    m_kernelPathEdit->setText(kernelPath);

    const QString version = queryKernelVersion(kernelPath);
    if (version.isEmpty()) {
        m_kernelVersionLabel->setText(tr("Not installed"));
        m_kernelVersionLabel->setStyleSheet("color: #e94560; font-weight: bold;");
    } else {
        m_kernelVersionLabel->setText(version);
        m_kernelVersionLabel->setStyleSheet("color: #4ecca3; font-weight: bold;");
    }
}

void SettingsView::fetchKernelVersions()
{
    if (!m_httpClient) {
        return;
    }

    const QStringList apiUrls = kernelReleasesApiUrls();
    auto tryFetch = std::make_shared<std::function<void(int)>>();
    *tryFetch = [this, apiUrls, tryFetch](int index) {
        if (index >= apiUrls.size()) {
            Logger::warn(tr("Failed to fetch kernel version list"));
            return;
        }

        const QString url = apiUrls.at(index);
        m_httpClient->get(url, [this, apiUrls, index, tryFetch](bool success, const QByteArray &data) {
            if (!success) {
                (*tryFetch)(index + 1);
                return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isArray()) {
                (*tryFetch)(index + 1);
                return;
            }

            QStringList versions;
            const QJsonArray arr = doc.array();
            for (const QJsonValue &item : arr) {
                const QJsonObject obj = item.toObject();
                const bool prerelease = obj.value("prerelease").toBool(false);
                if (prerelease) {
                    continue;
                }
                const QString tag = obj.value("tag_name").toString();
                if (tag.isEmpty() || isPreReleaseTag(tag)) {
                    continue;
                }
                versions.append(normalizeVersionTag(tag));
            }

            if (versions.isEmpty()) {
                (*tryFetch)(index + 1);
                return;
            }

            m_latestKernelVersion = versions.first();
            m_kernelVersionCombo->clear();
            m_kernelVersionCombo->addItem(tr("Latest version"));
            for (const QString &ver : versions) {
                m_kernelVersionCombo->addItem(ver);
            }
        });
    };

    (*tryFetch)(0);
}

void SettingsView::startKernelDownload(const QString &version)
{
    QString targetVersion = version.trimmed();
    if (targetVersion.isEmpty()) {
        targetVersion = m_latestKernelVersion.trimmed();
    }

    if (targetVersion.isEmpty()) {
        QMessageBox::warning(this, tr("Notice"), tr("Please check the kernel version list first"));
        return;
    }

    setDownloadUi(true, tr("Preparing to download kernel..."));

    const QString filename = buildKernelFilename(targetVersion);
    if (filename.isEmpty()) {
        setDownloadUi(false, tr("Unsupported system architecture"));
        return;
    }

    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/sing-box";
    QDir().mkpath(tempDir);

    const QString zipPath = tempDir + "/" + filename;
    const QString extractDir = tempDir + "/extract-" + targetVersion;

    const QStringList urls = buildDownloadUrls(targetVersion, filename);
    if (urls.isEmpty()) {
        setDownloadUi(false, tr("Download URL is empty"));
        return;
    }

    tryDownloadUrl(0, urls, zipPath, extractDir, targetVersion);
}

void SettingsView::tryDownloadUrl(int index, const QStringList &urls, const QString &savePath,
                                  const QString &extractDir, const QString &version)
{
    if (index >= urls.size()) {
        setDownloadUi(false, tr("Download failed, please try again"));
        QMessageBox::warning(this, tr("Download Failed"), tr("Failed to download kernel from mirror"));
        return;
    }

    const QString url = urls.at(index);
    m_kernelDownloadStatus->setText(tr("Downloading: %1").arg(url));

    m_httpClient->download(url, savePath,
        [this](qint64 received, qint64 total) {
            if (total > 0) {
                int percent = static_cast<int>((received * 100) / total);
                m_kernelDownloadProgress->setValue(percent);
            }
        },
        [this, urls, index, savePath, extractDir, version](bool success, const QByteArray &) {
            if (!success) {
                tryDownloadUrl(index + 1, urls, savePath, extractDir, version);
                return;
            }

            QString errorMessage;
            if (!extractZipArchive(savePath, extractDir, &errorMessage)) {
                setDownloadUi(false, tr("Extract failed: %1").arg(errorMessage));
                QMessageBox::warning(this, tr("Extract Failed"), errorMessage);
                return;
            }

            QString exeName = QSysInfo::productType() == "windows" ? "sing-box.exe" : "sing-box";
            const QString foundExe = findExecutableInDir(extractDir, exeName);
            if (foundExe.isEmpty()) {
                setDownloadUi(false, tr("Kernel file not found"));
                QMessageBox::warning(this, tr("Install Failed"), tr("sing-box executable not found in archive"));
                return;
            }

            ProcessManager::killProcessByName(exeName);

            const QString dataDir = kernelInstallDir();
            QDir().mkpath(dataDir);

            const QString destPath = dataDir + "/" + exeName;
            const QString backupPath = destPath + ".old";
            if (QFile::exists(destPath)) {
                QFile::remove(backupPath);
                QFile::rename(destPath, backupPath);
            }

            if (!QFile::copy(foundExe, destPath)) {
                setDownloadUi(false, tr("Install failed: cannot write kernel file"));
                QMessageBox::warning(this, tr("Install Failed"), tr("Failed to write kernel file"));
                return;
            }
#ifndef Q_OS_WIN
            QFile::setPermissions(destPath,
                QFile::permissions(destPath)
                | QFileDevice::ExeOwner
                | QFileDevice::ExeGroup
                | QFileDevice::ExeOther);
#endif

            setDownloadUi(false, tr("Download complete"));
            QMessageBox::information(this, tr("Done"), tr("Kernel downloaded and installed successfully"));
            refreshKernelInfo();
        }
    );
}

QString SettingsView::detectKernelPath() const
{
#ifdef Q_OS_WIN
    const QString kernelName = "sing-box.exe";
#else
    const QString kernelName = "sing-box";
#endif

    const QString dataDir = kernelInstallDir();
    const QString localPath = dataDir + "/" + kernelName;
    if (QFile::exists(localPath)) {
        return localPath;
    }

#if defined(Q_OS_FREEBSD)
    const QString usrLocalPath = "/usr/local/bin/" + kernelName;
    if (QFile::exists(usrLocalPath)) {
        return usrLocalPath;
    }

    const QString usrPath = "/usr/bin/" + kernelName;
    if (QFile::exists(usrPath)) {
        return usrPath;
    }
#endif

    const QString envPath = QStandardPaths::findExecutable(kernelName);
    if (!envPath.isEmpty() && QFile::exists(envPath)) {
        return envPath;
    }

    return QString();
}

QString SettingsView::queryKernelVersion(const QString &kernelPath) const
{
    if (kernelPath.isEmpty() || !QFile::exists(kernelPath)) {
        return QString();
    }

    QProcess process;
    process.start(kernelPath, {"version"});
    if (!process.waitForFinished(3000)) {
        process.kill();
        return QString();
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    QRegularExpression re("(\\d+\\.\\d+\\.\\d+)");
    QRegularExpressionMatch match = re.match(output);
    if (match.hasMatch()) {
        return match.captured(1);
    }

    return output;
}

QString SettingsView::getKernelArch() const
{
    const QString arch = QSysInfo::currentCpuArchitecture().toLower();
    if (arch.contains("arm64") || arch.contains("aarch64")) {
        return "arm64";
    }
    if (arch.contains("amd64") || arch.contains("x86_64") || arch.contains("x64") || arch.contains("64")) {
        return "amd64";
    }
    return "386";
}

QString SettingsView::buildKernelFilename(const QString &version) const
{
    const QString arch = getKernelArch();
    if (arch.isEmpty()) {
        return QString();
    }

    QString cleanVersion = version;
    if (cleanVersion.startsWith('v')) {
        cleanVersion = cleanVersion.mid(1);
    }

#ifdef Q_OS_WIN
    bool useLegacy = false;
    const QOperatingSystemVersion osVersion = QOperatingSystemVersion::current();
    if (osVersion.majorVersion() > 0 && osVersion.majorVersion() < 10) {
        useLegacy = true;
    }

    if (useLegacy && (arch == "amd64" || arch == "386")) {
        return QString("sing-box-%1-windows-%2-legacy-windows-7.zip").arg(cleanVersion, arch);
    }

    return QString("sing-box-%1-windows-%2.zip").arg(cleanVersion, arch);
#elif defined(Q_OS_FREEBSD)
    return QString("sing-box-%1-freebsd-%2.tar.gz").arg(cleanVersion, arch);
#elif defined(Q_OS_MACOS)
    return QString("sing-box-%1-darwin-%2.tar.gz").arg(cleanVersion, arch);
#else
    return QString("sing-box-%1-linux-%2.tar.gz").arg(cleanVersion, arch);
#endif
}

QStringList SettingsView::buildDownloadUrls(const QString &version, const QString &filename) const
{
    QString tag = version;
    if (!tag.startsWith('v')) {
        tag = "v" + tag;
    }

    const QString base = QString("https://github.com/SagerNet/sing-box/releases/download/%1/%2").arg(tag, filename);

    QStringList urls;
    urls << base;
    urls << "https://ghproxy.com/" + base;
    urls << "https://mirror.ghproxy.com/" + base;
    urls << "https://ghproxy.net/" + base;
    return urls;
}

QString SettingsView::findExecutableInDir(const QString &dirPath, const QString &exeName) const
{
    QDirIterator it(dirPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (QFileInfo(path).fileName().compare(exeName, Qt::CaseInsensitive) == 0) {
            return path;
        }
    }
    return QString();
}

bool SettingsView::extractZipArchive(const QString &zipPath, const QString &destDir, QString *errorMessage) const
{
#ifdef Q_OS_WIN
    QDir dest(destDir);
    if (dest.exists()) {
        dest.removeRecursively();
    }
    QDir().mkpath(destDir);

    const QString command = QString("Expand-Archive -Force -LiteralPath \"%1\" -DestinationPath \"%2\"")
        .arg(zipPath, destDir);

    QProcess proc;
    proc.start("powershell", {"-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", command});
    if (!proc.waitForFinished(300000)) {
        if (errorMessage) {
            *errorMessage = tr("Extraction timed out");
        }
        proc.kill();
        return false;
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            if (errorMessage->isEmpty()) {
                *errorMessage = tr("Extraction failed");
            }
        }
        return false;
    }

    return true;
#else
    QDir dest(destDir);
    if (dest.exists()) {
        dest.removeRecursively();
    }
    QDir().mkpath(destDir);

    QString tarPath = QStandardPaths::findExecutable("tar");
    if (tarPath.isEmpty()) {
        tarPath = QStandardPaths::findExecutable("bsdtar");
    }
    if (tarPath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("tar not found");
        }
        return false;
    }

    QProcess proc;
    proc.start(tarPath, {"-xf", zipPath, "-C", destDir});
    if (!proc.waitForFinished(300000)) {
        if (errorMessage) {
            *errorMessage = tr("Extraction timed out");
        }
        proc.kill();
        return false;
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            if (errorMessage->isEmpty()) {
                *errorMessage = tr("Extraction failed");
            }
        }
        return false;
    }

    return true;
#endif
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
