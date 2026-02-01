#ifndef SETTINGSVIEW_H
#define SETTINGSVIEW_H

#include <QWidget>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QProgressBar>
#include <QStringList>
#include <QString>

#include "models/SettingsModel.h"
#include "services/settings/SettingsService.h"

class ToggleSwitch;
class MenuComboBox;
class KernelManager;
class ThemeService;

class SettingsView : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsView(ThemeService *themeService, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onSaveClicked();
    void onDownloadKernelClicked();
    void onCheckKernelClicked();
    void onCheckUpdateClicked();
    void updateStyle();
    void onKernelInstalledReady(const QString &path, const QString &version);
    void onKernelReleasesReady(const QStringList &versions, const QString &latest);
    void onKernelLatestReady(const QString &latest, const QString &installed);
    void onKernelDownloadProgress(int percent);
    void onKernelStatusChanged(const QString &status);
    void onKernelFinished(bool ok, const QString &message);

private:
    void setupUI();
    void loadSettings();
    bool saveSettings();
    void setDownloadUi(bool downloading, const QString &message = QString());
    void ensureKernelInfoLoaded();
    QWidget* buildProxySection();
    QWidget* buildProxyAdvancedSection();
    QWidget* buildProfileSection();
    QWidget* buildAppearanceSection();
    QWidget* buildKernelSection();
    QLabel* createSectionTitle(const QString &text);
    QFrame* createCard();
    QLabel* createFormLabel(const QString &text);
    void matchLabelWidth(QLabel *left, QLabel *right);
    void applySettingsToUi(const SettingsModel::Data &data);
    void fillGeneralFromUi(SettingsModel::Data &data) const;
    void fillAdvancedFromUi(SettingsModel::Data &data) const;
    void fillProfileFromUi(SettingsModel::Data &data) const;
    QString normalizeBypassText(const QString &text) const;

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

    // Subscription profile (advanced).
    MenuComboBox *m_defaultOutboundCombo;
    MenuComboBox *m_downloadDetourCombo;
    ToggleSwitch *m_blockAdsSwitch;
    ToggleSwitch *m_dnsHijackSwitch;
    ToggleSwitch *m_enableAppGroupsSwitch;
    QLineEdit *m_dnsProxyEdit;
    QLineEdit *m_dnsCnEdit;
    QLineEdit *m_dnsResolverEdit;
    QLineEdit *m_urltestUrlEdit;

    // Appearance settings.
    MenuComboBox *m_themeCombo;
    MenuComboBox *m_languageCombo;

    // Kernel settings.
    QLabel *m_kernelVersionLabel;
    MenuComboBox *m_kernelVersionCombo;
    QProgressBar *m_kernelDownloadProgress;
    QLabel *m_kernelDownloadStatus;
    QLineEdit *m_kernelPathEdit;
    QPushButton *m_downloadKernelBtn;
    QPushButton *m_checkKernelBtn;
    QPushButton *m_checkUpdateBtn;
    bool m_isDownloading = false;
    bool m_checkingInstall = false;
    KernelManager *m_kernelManager;
    bool m_kernelInfoLoaded = false;
    QString m_inputStyleApplied;
    QString m_comboStyle;

    QPushButton *m_saveBtn;
    ThemeService *m_themeService = nullptr;
};

#endif // SETTINGSVIEW_H
