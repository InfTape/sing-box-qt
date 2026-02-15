#ifndef SETTINGSVIEW_H
#define SETTINGSVIEW_H
#include <QCheckBox>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QToolButton>
#include <QWidget>
#include "models/SettingsModel.h"
class ToggleSwitch;
class MenuComboBox;
class ElideLineEdit;
class SettingsController;
class ThemeService;
class QResizeEvent;

class SettingsView : public QWidget {
  Q_OBJECT
 public:
  explicit SettingsView(ThemeService*       themeService,
                        SettingsController* controller = nullptr,
                        QWidget*            parent     = nullptr);

 protected:
  void showEvent(QShowEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
 private slots:
  void onKernelStatusIconClicked();
  void updateStyle();
  void onKernelInstalledReady(const QString& path, const QString& version);
  void onKernelReleasesReady(const QStringList& versions,
                             const QString&     latest);
  void onKernelDownloadProgress(int percent);
  void onKernelStatusChanged(const QString& status);
  void onKernelFinished(bool ok, const QString& message);

 private:
  void      setupUI();
  void      setupAutoSave();
  void      scheduleAutoSave();
  void      loadSettings();
  bool      saveSettings(bool showError);
  void      setDownloadUi(bool           downloading,
                          const QString& message   = QString(),
                          const QString& hintLevel = QString());
  void      ensureKernelInfoLoaded();
  QWidget*  buildProxySection();
  QWidget*  buildProxyAdvancedSection();
  QWidget*  buildProfileSection();
  QWidget*  buildAppearanceSection();
  QWidget*  buildKernelSection();
  QLabel*   createSectionTitle(const QString& text);
  QFrame*   createCard();
  QLabel*   createFormLabel(const QString& text);
  QSpinBox* createSpinBox(int min, int max, int value, bool blockWheel);
  MenuComboBox*  createMenuComboBox(bool expanding = false);
  ElideLineEdit* createElideLineEdit(const QString& placeholder);
  void           prepareFormLabelPair(QLabel* left, QLabel* right);
  void           applySettingsToUi(const SettingsModel::Data& data);
  void           fillGeneralFromUi(SettingsModel::Data& data) const;
  void           fillAdvancedFromUi(SettingsModel::Data& data) const;
  void           fillProfileFromUi(SettingsModel::Data& data) const;
  QString        normalizeBypassText(const QString& text) const;
  void           updateKernelVersionLabelStatus();
  QIcon          buildKernelStatusIcon(const QString& resourcePath,
                                       int rotationDegrees = 0) const;
  void           setKernelStatusIcon(const QString& resourcePath);
  void           startKernelStatusIconSpin();
  void           stopKernelStatusIconSpin();
  void           advanceKernelStatusIconSpin();
  void           updateResponsiveUi();
  // Proxy settings.
  QSpinBox*       m_mixedPortSpin;
  QSpinBox*       m_apiPortSpin;
  QCheckBox*      m_autoStartCheck;
  QPlainTextEdit* m_systemProxyBypassEdit;
  QSpinBox*       m_tunMtuSpin;
  MenuComboBox*   m_tunStackCombo;
  ToggleSwitch*   m_tunEnableIpv6Switch;
  ToggleSwitch*   m_tunAutoRouteSwitch;
  ToggleSwitch*   m_tunStrictRouteSwitch;
  // Subscription profile (advanced).
  QLabel*       m_defaultOutboundLabel = nullptr;
  QLabel*       m_downloadDetourLabel  = nullptr;
  MenuComboBox* m_defaultOutboundCombo;
  MenuComboBox* m_downloadDetourCombo;
  ToggleSwitch* m_blockAdsSwitch;
  ToggleSwitch* m_dnsHijackSwitch;
  ToggleSwitch* m_enableAppGroupsSwitch;
  QLabel*       m_dnsResolverLabel = nullptr;
  QLabel*       m_urltestLabel     = nullptr;
  QLineEdit*    m_dnsProxyEdit;
  QLineEdit*    m_dnsCnEdit;
  QLineEdit*    m_dnsResolverEdit;
  QLineEdit*    m_urltestUrlEdit;
  // Appearance settings.
  MenuComboBox* m_themeCombo;
  MenuComboBox* m_languageCombo;
  // Kernel settings.
  QLabel*             m_kernelVersionLabel;
  QLabel*             m_kernelVersionHintLabel = nullptr;
  QString             m_installedKernelVersion;
  QString             m_latestKernelVersion;
  QToolButton*        m_kernelStatusIconBtn = nullptr;
  MenuComboBox*       m_kernelVersionCombo;
  QProgressBar*       m_kernelDownloadProgress;
  QLineEdit*          m_kernelPathEdit;
  QTimer*             m_kernelStatusSpinTimer = nullptr;
  int                 m_kernelStatusSpinAngle = 0;
  bool                m_kernelIsOutdated   = false;
  QString             m_kernelInlineHintText;
  QString             m_kernelInlineHintLevel = "info";
  bool                m_isDownloading      = false;
  SettingsController* m_settingsController = nullptr;
  bool                m_kernelInfoLoaded   = false;
  bool                m_isApplyingSettings = false;
  QString             m_inputStyleApplied;
  QString             m_comboStyle;
  QTimer*             m_autoSaveTimer      = nullptr;
  ThemeService*       m_themeService = nullptr;
};
#endif  // SETTINGSVIEW_H
