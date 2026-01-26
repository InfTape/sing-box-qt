#ifndef SETTINGSVIEW_H
#define SETTINGSVIEW_H

#include <QWidget>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QProgressBar>
#include <QStringList>

class HttpClient;
class ToggleSwitch;
class MenuComboBox;

class SettingsView : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsView(QWidget *parent = nullptr);

private slots:
    void onSaveClicked();
    void onSaveAdvancedClicked();
    void onDownloadKernelClicked();
    void onCheckKernelClicked();
    void onCheckUpdateClicked();
    void updateStyle();

private:
    void setupUI();
    void loadSettings();
    void saveSettings();
    void refreshKernelInfo();
    void fetchKernelVersions();
    void startKernelDownload(const QString &version);
    void tryDownloadUrl(int index, const QStringList &urls, const QString &savePath, const QString &extractDir, const QString &version);
    QString detectKernelPath() const;
    QString queryKernelVersion(const QString &kernelPath) const;
    QString getKernelArch() const;
    QString buildKernelFilename(const QString &version) const;
    QStringList buildDownloadUrls(const QString &version, const QString &filename) const;
    QString findExecutableInDir(const QString &dirPath, const QString &exeName) const;
    bool extractZipArchive(const QString &zipPath, const QString &destDir, QString *errorMessage) const;
    void setDownloadUi(bool downloading, const QString &message = QString());

    // Proxy settings.
    QSpinBox *m_mixedPortSpin;
    QSpinBox *m_apiPortSpin;
    QCheckBox *m_autoStartCheck;
    QCheckBox *m_systemProxyCheck;
    QPlainTextEdit *m_systemProxyBypassEdit;
    QSpinBox *m_tunMtuSpin;
    MenuComboBox *m_tunStackCombo;
    ToggleSwitch *m_tunEnableIpv6Switch;
    ToggleSwitch *m_tunAutoRouteSwitch;
    ToggleSwitch *m_tunStrictRouteSwitch;
    QPushButton *m_saveAdvancedBtn;

    // Appearance settings.
    QComboBox *m_themeCombo;
    QComboBox *m_languageCombo;

    // Kernel settings.
    QLabel *m_kernelVersionLabel;
    QComboBox *m_kernelVersionCombo;
    QProgressBar *m_kernelDownloadProgress;
    QLabel *m_kernelDownloadStatus;
    QLineEdit *m_kernelPathEdit;
    QPushButton *m_downloadKernelBtn;
    QPushButton *m_checkKernelBtn;
    QPushButton *m_checkUpdateBtn;
    HttpClient *m_httpClient;
    QString m_latestKernelVersion;
    bool m_isDownloading = false;

    QPushButton *m_saveBtn;
};

#endif // SETTINGSVIEW_H
