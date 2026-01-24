#include "SettingsView.h"
#include "storage/DatabaseService.h"
#include "system/UpdateService.h"
#include "system/AutoStart.h"
#include "utils/Logger.h"
#include "network/HttpClient.h"
#include "core/ProcessManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
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
#include <functional>
#include "utils/AppPaths.h"
#include "utils/ThemeManager.h"
#include "widgets/MenuComboBox.h"

namespace {


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
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(30, 30, 30, 30);
    mainLayout->setSpacing(20);

    ThemeManager &tm = ThemeManager::instance();
    QString groupBoxStyle = QString(R"(
        QGroupBox {
            background-color: %1;
            border: none;
            border-radius: 10px;
            margin-top: 20px;
            padding: 20px;
            padding-top: 34px;
            font-weight: bold;
            color: #eaeaea;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0;
        }
    )").arg(tm.getColorString("panel-bg"));

    QString inputStyle = R"(
        QSpinBox, QLineEdit {
            background-color: %1;
            border: none;
            border-radius: 10px;
            padding: 8px 12px;
            color: #eaeaea;
            min-width: 150px;
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
    )";
    const QString inputStyleApplied = inputStyle.arg(tm.getColorString("bg-secondary"));


    QGroupBox *proxyGroup = new QGroupBox(tr("Proxy Settings"));
    proxyGroup->setStyleSheet(groupBoxStyle);
    QFormLayout *proxyLayout = new QFormLayout(proxyGroup);
    proxyLayout->setSpacing(15);

    m_mixedPortSpin = new QSpinBox;
    m_mixedPortSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_mixedPortSpin->setRange(1, 65535);
    m_mixedPortSpin->setValue(7890);
    m_mixedPortSpin->setStyleSheet(inputStyleApplied);

    m_apiPortSpin = new QSpinBox;
    m_apiPortSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_apiPortSpin->setRange(1, 65535);
    m_apiPortSpin->setValue(9090);
    m_apiPortSpin->setStyleSheet(inputStyleApplied);

    m_autoStartCheck = new QCheckBox(tr("Auto start on boot"));
    m_autoStartCheck->setStyleSheet(inputStyleApplied);

    m_systemProxyCheck = new QCheckBox(tr("Auto-set system proxy"));
    m_systemProxyCheck->setStyleSheet(inputStyleApplied);

    proxyLayout->addRow(tr("Mixed port:"), m_mixedPortSpin);
    proxyLayout->addRow(tr("API port:"), m_apiPortSpin);
    proxyLayout->addRow(m_autoStartCheck);
    proxyLayout->addRow(m_systemProxyCheck);


    QGroupBox *appearanceGroup = new QGroupBox(tr("Appearance"));
    appearanceGroup->setStyleSheet(groupBoxStyle);
    QFormLayout *appearanceLayout = new QFormLayout(appearanceGroup);
    appearanceLayout->setSpacing(15);

    m_themeCombo = new MenuComboBox;
    m_themeCombo->addItems({tr("Dark"), tr("Light"), tr("Follow System")});

    m_languageCombo = new MenuComboBox;
    m_languageCombo->addItems({tr("Simplified Chinese"), "English", tr("Japanese"), tr("Russian")});

    appearanceLayout->addRow(tr("Theme:"), m_themeCombo);
    appearanceLayout->addRow(tr("Language:"), m_languageCombo);


    QGroupBox *kernelGroup = new QGroupBox(tr("Kernel Settings"));
    kernelGroup->setStyleSheet(groupBoxStyle);
    QFormLayout *kernelLayout = new QFormLayout(kernelGroup);
    kernelLayout->setSpacing(15);

    m_kernelVersionLabel = new QLabel(tr("Not installed"));
    m_kernelVersionLabel->setStyleSheet("color: #e94560; font-weight: bold;");

    m_kernelVersionCombo = new MenuComboBox;
    m_kernelVersionCombo->addItem(tr("Latest version"));

    m_kernelPathEdit = new QLineEdit;
    m_kernelPathEdit->setReadOnly(true);
    m_kernelPathEdit->setPlaceholderText(tr("Kernel path"));
    m_kernelPathEdit->setStyleSheet(inputStyleApplied);

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

    kernelLayout->addRow(tr("Installed version:"), m_kernelVersionLabel);
    kernelLayout->addRow(tr("Select version:"), m_kernelVersionCombo);
    kernelLayout->addRow(tr("Kernel path:"), m_kernelPathEdit);
    kernelLayout->addRow(m_kernelDownloadProgress);
    kernelLayout->addRow(m_kernelDownloadStatus);
    kernelLayout->addRow(kernelBtnLayout);

    m_saveBtn = new QPushButton(tr("Save"));
    m_saveBtn->setFixedHeight(36);
    m_saveBtn->setFixedWidth(110);

    mainLayout->addWidget(proxyGroup);
    mainLayout->addWidget(appearanceGroup);
    mainLayout->addWidget(kernelGroup);
    mainLayout->addStretch();
    mainLayout->addWidget(m_saveBtn, 0, Qt::AlignHCenter);

    connect(m_saveBtn, &QPushButton::clicked, this, &SettingsView::onSaveClicked);
    connect(m_downloadKernelBtn, &QPushButton::clicked, this, &SettingsView::onDownloadKernelClicked);
    connect(m_checkKernelBtn, &QPushButton::clicked, this, &SettingsView::onCheckKernelClicked);
    connect(m_checkUpdateBtn, &QPushButton::clicked, this, &SettingsView::onCheckUpdateClicked);
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

    QJsonObject theme = DatabaseService::instance().getThemeConfig();
    QString themeName = theme.value("theme").toString("dark");
    if (themeName == "light") {
        m_themeCombo->setCurrentIndex(1);
    } else if (themeName == "auto") {
        m_themeCombo->setCurrentIndex(2);
    } else {
        m_themeCombo->setCurrentIndex(0);
    }

    QString locale = DatabaseService::instance().getLocale();
    if (locale == "en") {
        m_languageCombo->setCurrentIndex(1);
    } else if (locale == "ja") {
        m_languageCombo->setCurrentIndex(2);
    } else if (locale == "ru") {
        m_languageCombo->setCurrentIndex(3);
    } else {
        m_languageCombo->setCurrentIndex(0);
    }
}

void SettingsView::saveSettings()
{
    QJsonObject config;
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
