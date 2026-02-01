#ifndef SETTINGSCONTROLLER_H
#define SETTINGSCONTROLLER_H

#include <QObject>
#include <QStringList>

#include "models/SettingsModel.h"

class KernelManager;
class SettingsController : public QObject {
  Q_OBJECT

 public:
  explicit SettingsController(QObject* parent = nullptr);
  ~SettingsController() override = default;

  SettingsModel::Data loadSettings() const;
  bool saveSettings(const SettingsModel::Data& data, int themeIndex,
                    int languageIndex, QString* errorMessage = nullptr) const;

  void refreshInstalledInfo();
  void fetchReleaseList();
  void checkLatest();
  void downloadAndInstall(const QString& version);

 signals:
  void installedInfoReady(const QString& path, const QString& version);
  void releasesReady(const QStringList& versions, const QString& latest);
  void latestReady(const QString& latest, const QString& installed);
  void downloadProgress(int percent);
  void statusChanged(const QString& status);
  void finished(bool ok, const QString& message);

 private:
  KernelManager* m_kernelManager = nullptr;
};

#endif  // SETTINGSCONTROLLER_H
