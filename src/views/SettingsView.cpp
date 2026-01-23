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
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <functional>
#include "utils/AppPaths.h"
#include "utils/ThemeManager.h"

namespace {
class RoundedMenu : public QMenu
{
public:
    explicit RoundedMenu(QWidget *parent = nullptr)
        : QMenu(parent)
    {
        setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
    }

    void setThemeColors(const QColor &bg, const QColor &border)
    {
        m_bgColor = bg;
        m_borderColor = border;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QRectF rect = this->rect().adjusted(1, 1, -1, -1);
        QPainterPath path;
        path.addRoundedRect(rect, 10, 10);

        painter.fillPath(path, m_bgColor);
        painter.setPen(QPen(m_borderColor, 1));
        painter.drawPath(path);

        QMenu::paintEvent(event);
    }

private:
    QColor m_bgColor = QColor(30, 41, 59);
    QColor m_borderColor = QColor(90, 169, 255, 180);
};

class MenuComboBox : public QComboBox
{
public:
    explicit MenuComboBox(QWidget *parent = nullptr)
        : QComboBox(parent)
    {
        m_menu = new RoundedMenu(this);
        m_menu->setObjectName("ComboMenu");
        updateMenuStyle();

        ThemeManager &tm = ThemeManager::instance();
        connect(&tm, &ThemeManager::themeChanged, this, [this]() {
            updateMenuStyle();
        });
    }

protected:
    void showPopup() override
    {
        if (!m_menu) return;
        m_menu->clear();

        for (int i = 0; i < count(); ++i) {
            QAction *action = m_menu->addAction(itemText(i));
            action->setCheckable(true);
            action->setChecked(i == currentIndex());
            connect(action, &QAction::triggered, this, [this, i]() {
                setCurrentIndex(i);
            });
        }

        const int menuWidth = qMax(width(), 180);
        m_menu->setFixedWidth(menuWidth);
        m_menu->popup(mapToGlobal(QPoint(0, height())));
    }

    void hidePopup() override
    {
        if (m_menu) {
            m_menu->hide();
        }
    }

private:
    void updateMenuStyle()
    {
        if (!m_menu) return;
        ThemeManager &tm = ThemeManager::instance();
        m_menu->setThemeColors(tm.getColor("bg-secondary"), tm.getColor("primary"));
        m_menu->setStyleSheet(QString(R"(
            #ComboMenu {
                background: transparent;
                border: none;
                padding: 6px;
            }
            #ComboMenu::panel {
                background: transparent;
                border: none;
            }
            #ComboMenu::item {
                color: %1;
                padding: 8px 14px;
                border-radius: 10px;
            }
            #ComboMenu::indicator {
                width: 14px;
                height: 14px;
            }
            #ComboMenu::indicator:checked {
                image: url(:/icons/check.svg);
            }
            #ComboMenu::indicator:unchecked {
                image: none;
            }
            #ComboMenu::item:selected {
                background-color: %2;
                color: white;
            }
            #ComboMenu::item:checked {
                color: %4;
            }
            #ComboMenu::item:checked:selected {
                color: %4;
            }
            #ComboMenu::separator {
                height: 1px;
                background-color: %3;
                margin: 6px 4px;
            }
        )")
        .arg(tm.getColorString("text-primary"))
        .arg(tm.getColorString("bg-tertiary"))
        .arg(tm.getColorString("border"))
        .arg(tm.getColorString("primary")));
    }

    RoundedMenu *m_menu = nullptr;
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

    // 代理设置
    QGroupBox *proxyGroup = new QGroupBox(tr("代理设置"));
    proxyGroup->setStyleSheet(groupBoxStyle);
    QFormLayout *proxyLayout = new QFormLayout(proxyGroup);
    proxyLayout->setSpacing(15);

    m_mixedPortSpin = new QSpinBox;
    m_mixedPortSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_mixedPortSpin->setRange(1, 65535);
    m_mixedPortSpin->setValue(7890);
    m_mixedPortSpin->setStyleSheet(inputStyle);

    m_apiPortSpin = new QSpinBox;
    m_apiPortSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_apiPortSpin->setRange(1, 65535);
    m_apiPortSpin->setValue(9090);
    m_apiPortSpin->setStyleSheet(inputStyle);

    m_autoStartCheck = new QCheckBox(tr("开机自动启动"));
    m_autoStartCheck->setStyleSheet(inputStyle);

    m_systemProxyCheck = new QCheckBox(tr("自动设置系统代理"));
    m_systemProxyCheck->setStyleSheet(inputStyle);

    proxyLayout->addRow(tr("混合代理端口:"), m_mixedPortSpin);
    proxyLayout->addRow(tr("API 端口:"), m_apiPortSpin);
    proxyLayout->addRow(m_autoStartCheck);
    proxyLayout->addRow(m_systemProxyCheck);

    // 外观设置
    QGroupBox *appearanceGroup = new QGroupBox(tr("外观设置"));
    appearanceGroup->setStyleSheet(groupBoxStyle);
    QFormLayout *appearanceLayout = new QFormLayout(appearanceGroup);
    appearanceLayout->setSpacing(15);

    m_themeCombo = new MenuComboBox;
    m_themeCombo->addItems({tr("深色"), tr("浅色"), tr("跟随系统")});

    m_languageCombo = new MenuComboBox;
    m_languageCombo->addItems({tr("简体中文"), "English", tr("日本語"), tr("Русский")});

    appearanceLayout->addRow(tr("主题:"), m_themeCombo);
    appearanceLayout->addRow(tr("语言:"), m_languageCombo);

    // 内核设置
    QGroupBox *kernelGroup = new QGroupBox(tr("内核设置"));
    kernelGroup->setStyleSheet(groupBoxStyle);
    QFormLayout *kernelLayout = new QFormLayout(kernelGroup);
    kernelLayout->setSpacing(15);

    m_kernelVersionLabel = new QLabel(tr("未安装"));
    m_kernelVersionLabel->setStyleSheet("color: #e94560; font-weight: bold;");

    m_kernelVersionCombo = new MenuComboBox;
    m_kernelVersionCombo->addItem(tr("最新版本"));

    m_kernelPathEdit = new QLineEdit;
    m_kernelPathEdit->setReadOnly(true);
    m_kernelPathEdit->setPlaceholderText(tr("内核路径"));
    m_kernelPathEdit->setStyleSheet(inputStyle);

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

    m_downloadKernelBtn = new QPushButton(tr("下载内核"));
    m_downloadKernelBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #e94560;
            color: white;
            border: none;
            padding: 10px 20px;
            border-radius: 10px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #ff6b6b; }
    )");

    m_checkKernelBtn = new QPushButton(tr("检查安装"));
    m_checkKernelBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #0f3460;
            color: white;
            border: none;
            padding: 10px 20px;
            border-radius: 10px;
        }
        QPushButton:hover { background-color: #1f4068; }
    )");

    m_checkUpdateBtn = new QPushButton(tr("检查更新"));
    m_checkUpdateBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #0f3460;
            color: white;
            border: none;
            padding: 10px 20px;
            border-radius: 10px;
        }
        QPushButton:hover { background-color: #1f4068; }
    )");

    kernelBtnLayout->addWidget(m_downloadKernelBtn);
    kernelBtnLayout->addWidget(m_checkKernelBtn);
    kernelBtnLayout->addWidget(m_checkUpdateBtn);
    kernelBtnLayout->addStretch();

    kernelLayout->addRow(tr("已安装版本:"), m_kernelVersionLabel);
    kernelLayout->addRow(tr("选择版本:"), m_kernelVersionCombo);
    kernelLayout->addRow(tr("内核路径:"), m_kernelPathEdit);
    kernelLayout->addRow(m_kernelDownloadProgress);
    kernelLayout->addRow(m_kernelDownloadStatus);
    kernelLayout->addRow(kernelBtnLayout);

    m_saveBtn = new QPushButton(tr("保存设置"));
    m_saveBtn->setFixedHeight(36);
    m_saveBtn->setFixedWidth(110);
    m_saveBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #6ee7b7;
            color: white;
            border: none;
            border-radius: 10px;
            font-size: 16px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #6ee7b7ae; }
    )");

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
            QMessageBox::warning(this, tr("提示"), tr("设置开机自启动失败"));
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

    Logger::info(tr("设置已保存"));
}

void SettingsView::onSaveClicked()
{
    saveSettings();
    QMessageBox::information(this, tr("提示"), tr("设置已保存"));
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
            QMessageBox::warning(this, tr("检查更新"), tr("获取内核版本失败，请稍后重试"));
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
                QMessageBox::information(this, tr("检查更新"),
                    tr("当前未安装内核，最新版本为 %1").arg(latest));
                return;
            }

            if (installed == latest) {
                QMessageBox::information(this, tr("检查更新"), tr("当前已是最新版本"));
                return;
            }

            QMessageBox::information(this, tr("检查更新"),
                tr("发现内核新版本 %1，当前版本 %2").arg(latest, installed));
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
        m_kernelVersionLabel->setText(tr("未安装"));
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
            Logger::warn(tr("获取内核版本列表失败"));
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
            m_kernelVersionCombo->addItem(tr("最新版本"));
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
        QMessageBox::warning(this, tr("提示"), tr("请先检查内核版本列表"));
        return;
    }

    setDownloadUi(true, tr("准备下载内核..."));

    const QString filename = buildKernelFilename(targetVersion);
    if (filename.isEmpty()) {
        setDownloadUi(false, tr("无法识别系统架构"));
        return;
    }

    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/sing-box";
    QDir().mkpath(tempDir);

    const QString zipPath = tempDir + "/" + filename;
    const QString extractDir = tempDir + "/extract-" + targetVersion;

    const QStringList urls = buildDownloadUrls(targetVersion, filename);
    if (urls.isEmpty()) {
        setDownloadUi(false, tr("下载地址为空"));
        return;
    }

    tryDownloadUrl(0, urls, zipPath, extractDir, targetVersion);
}

void SettingsView::tryDownloadUrl(int index, const QStringList &urls, const QString &savePath,
                                  const QString &extractDir, const QString &version)
{
    if (index >= urls.size()) {
        setDownloadUi(false, tr("下载失败，请稍后重试"));
        QMessageBox::warning(this, tr("下载失败"), tr("无法从镜像下载内核"));
        return;
    }

    const QString url = urls.at(index);
    m_kernelDownloadStatus->setText(tr("正在下载：%1").arg(url));

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
                setDownloadUi(false, tr("解压失败：%1").arg(errorMessage));
                QMessageBox::warning(this, tr("解压失败"), errorMessage);
                return;
            }

            QString exeName = QSysInfo::productType() == "windows" ? "sing-box.exe" : "sing-box";
            const QString foundExe = findExecutableInDir(extractDir, exeName);
            if (foundExe.isEmpty()) {
                setDownloadUi(false, tr("未找到内核文件"));
                QMessageBox::warning(this, tr("安装失败"), tr("未在压缩包中找到 sing-box 可执行文件"));
                return;
            }

            ProcessManager::killProcessByName("sing-box.exe");

            const QString dataDir = kernelInstallDir();
            QDir().mkpath(dataDir);

            const QString destPath = dataDir + "/" + exeName;
            const QString backupPath = destPath + ".old";
            if (QFile::exists(destPath)) {
                QFile::remove(backupPath);
                QFile::rename(destPath, backupPath);
            }

            if (!QFile::copy(foundExe, destPath)) {
                setDownloadUi(false, tr("安装失败，无法写入内核文件"));
                QMessageBox::warning(this, tr("安装失败"), tr("无法写入内核文件"));
                return;
            }

            setDownloadUi(false, tr("下载完成"));
            QMessageBox::information(this, tr("完成"), tr("内核下载并安装成功"));
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
    const QString path = dataDir + "/" + kernelName;
    if (QFile::exists(path)) {
        return path;
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

    bool useLegacy = false;
#ifdef Q_OS_WIN
    const QOperatingSystemVersion osVersion = QOperatingSystemVersion::current();
    if (osVersion.majorVersion() > 0 && osVersion.majorVersion() < 10) {
        useLegacy = true;
    }
#endif

    if (useLegacy && (arch == "amd64" || arch == "386")) {
        return QString("sing-box-%1-windows-%2-legacy-windows-7.zip").arg(cleanVersion, arch);
    }

    return QString("sing-box-%1-windows-%2.zip").arg(cleanVersion, arch);
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
            *errorMessage = tr("解压超时");
        }
        proc.kill();
        return false;
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            if (errorMessage->isEmpty()) {
                *errorMessage = tr("解压失败");
            }
        }
        return false;
    }

    return true;
#else
    Q_UNUSED(zipPath)
    Q_UNUSED(destDir)
    if (errorMessage) {
        *errorMessage = tr("当前平台不支持解压");
    }
    return false;
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
