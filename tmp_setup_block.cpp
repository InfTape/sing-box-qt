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

    QWidget *singboxProfileSection = new QWidget;
    QVBoxLayout *singboxProfileLayout = new QVBoxLayout(singboxProfileSection);
    singboxProfileLayout->setContentsMargins(0, 0, 0, 0);
    singboxProfileLayout->setSpacing(12);
    singboxProfileLayout->addWidget(makeSectionTitle(tr("Subscription Config Profile (Advanced)")));

    QFrame *singboxProfileCard = makeCard();
    QVBoxLayout *singboxProfileCardLayout = new QVBoxLayout(singboxProfileCard);
    singboxProfileCardLayout->setContentsMargins(20, 20, 20, 20);
    singboxProfileCardLayout->setSpacing(16);

    QLabel *routingTitle = new QLabel(tr("Routing & Downloads"));
    routingTitle->setStyleSheet("color: #cbd5e1; font-weight: bold;");
    singboxProfileCardLayout->addWidget(routingTitle);

    QGridLayout *routingGrid = new QGridLayout;
    routingGrid->setHorizontalSpacing(16);
    routingGrid->setVerticalSpacing(12);
    routingGrid->setColumnStretch(1, 1);
    routingGrid->setColumnStretch(3, 1);

    const QString comboStyle = QString(R"(
        QComboBox {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 10px;
            padding: 6px 12px;
            color: #eaeaea;
            min-width: 150px;
        }
    )").arg(tm.getColorString("bg-primary"),
            tm.getColorString("border"));

    m_defaultOutboundCombo = new MenuComboBox;
    m_defaultOutboundCombo->addItems({tr("Manual selector (recommended)"), tr("Auto select (URLTest)")});
    m_defaultOutboundCombo->setWheelEnabled(false);
    m_defaultOutboundCombo->setFixedHeight(kSpinBoxHeight);
    m_defaultOutboundCombo->setStyleSheet(comboStyle);

    m_downloadDetourCombo = new MenuComboBox;
    m_downloadDetourCombo->addItems({tr("Manual selector"), tr("Direct")});
    m_downloadDetourCombo->setWheelEnabled(false);
    m_downloadDetourCombo->setFixedHeight(kSpinBoxHeight);
    m_downloadDetourCombo->setStyleSheet(comboStyle);

    QLabel *defaultOutboundLabel = makeFormLabel(tr("Default outbound for non-CN traffic"));
    QLabel *downloadDetourLabel = makeFormLabel(tr("Rule-set/UI download detour"));
    matchLabelWidth(defaultOutboundLabel, downloadDetourLabel);

    routingGrid->addWidget(defaultOutboundLabel, 0, 0);
    routingGrid->addWidget(m_defaultOutboundCombo, 0, 1);
    routingGrid->addWidget(downloadDetourLabel, 0, 2);
    routingGrid->addWidget(m_downloadDetourCombo, 0, 3);

    singboxProfileCardLayout->addLayout(routingGrid);

    QWidget *profileToggleCard = new QWidget;
    profileToggleCard->setStyleSheet(QString("background-color: %1; border-radius: 12px;")
        .arg(tm.getColorString("bg-secondary")));
    QHBoxLayout *profileToggleLayout = new QHBoxLayout(profileToggleCard);
    profileToggleLayout->setContentsMargins(16, 10, 16, 10);
    profileToggleLayout->setSpacing(30);

    auto addProfileToggle = [profileToggleLayout](const QString &text, ToggleSwitch *&toggle) {
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
        profileToggleLayout->addWidget(item);
    };

    addProfileToggle(tr("Block ads (geosite-category-ads-all)"), m_blockAdsSwitch);
    addProfileToggle(tr("DNS hijack (hijack-dns)"), m_dnsHijackSwitch);
    addProfileToggle(tr("Enable app groups (TG/YouTube/Netflix/OpenAI)"), m_enableAppGroupsSwitch);
    profileToggleLayout->addStretch();
    singboxProfileCardLayout->addWidget(profileToggleCard);

    QLabel *dnsTitle = new QLabel(tr("DNS"));
    dnsTitle->setStyleSheet("color: #cbd5e1; font-weight: bold;");
    singboxProfileCardLayout->addWidget(dnsTitle);

    QGridLayout *dnsGrid = new QGridLayout;
    dnsGrid->setHorizontalSpacing(16);
    dnsGrid->setVerticalSpacing(12);
    dnsGrid->setColumnStretch(1, 1);
    dnsGrid->setColumnStretch(3, 1);

    m_dnsProxyEdit = new QLineEdit;
    m_dnsProxyEdit->setPlaceholderText(ConfigConstants::DEFAULT_DNS_PROXY);
    m_dnsProxyEdit->setStyleSheet(inputStyleApplied);
    m_dnsProxyEdit->setFixedHeight(kSpinBoxHeight);

    m_dnsCnEdit = new QLineEdit;
    m_dnsCnEdit->setPlaceholderText(ConfigConstants::DEFAULT_DNS_CN);
    m_dnsCnEdit->setStyleSheet(inputStyleApplied);
    m_dnsCnEdit->setFixedHeight(kSpinBoxHeight);

    m_dnsResolverEdit = new QLineEdit;
    m_dnsResolverEdit->setPlaceholderText(ConfigConstants::DEFAULT_DNS_RESOLVER);
    m_dnsResolverEdit->setStyleSheet(inputStyleApplied);
    m_dnsResolverEdit->setFixedHeight(kSpinBoxHeight);

    m_urltestUrlEdit = new QLineEdit;
    m_urltestUrlEdit->setPlaceholderText(ConfigConstants::DEFAULT_URLTEST_URL);
    m_urltestUrlEdit->setStyleSheet(inputStyleApplied);
    m_urltestUrlEdit->setFixedHeight(kSpinBoxHeight);

    QLabel *dnsProxyLabel = makeFormLabel(tr("Proxy DNS (non-CN)"));
    QLabel *dnsCnLabel = makeFormLabel(tr("CN DNS"));
    matchLabelWidth(dnsProxyLabel, dnsCnLabel);

    QLabel *dnsResolverLabel = makeFormLabel(tr("Resolver DNS (for DoH hostname resolving)"));
    QLabel *urltestLabel = makeFormLabel(tr("URLTest URL"));
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

    m_saveSingboxProfileBtn = new QPushButton(tr("Save Profile Settings"));
    singboxProfileCardLayout->addWidget(m_saveSingboxProfileBtn);

    singboxProfileLayout->addWidget(singboxProfileCard);

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
    mainLayout->addWidget(singboxProfileSection);
    mainLayout->addWidget(appearanceSection);
    mainLayout->addWidget(kernelSection);
    mainLayout->addStretch();
    mainLayout->addWidget(m_saveBtn, 0, Qt::AlignHCenter);

    scrollArea->setWidget(contentWidget);
    outerLayout->addWidget(scrollArea, 1);

    connect(m_saveBtn, &QPushButton::clicked, this, &SettingsView::onSaveClicked);
    connect(m_saveAdvancedBtn, &QPushButton::clicked, this, &SettingsView::onSaveAdvancedClicked);
    connect(m_saveSingboxProfileBtn, &QPushButton::clicked, this, &SettingsView::onSaveSingboxProfileClicked);
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
    applyTransparentStyle(m_saveSingboxProfileBtn, QColor("#3b82f6"));
    applyTransparentStyle(m_saveBtn, QColor("#10b981"));
}

