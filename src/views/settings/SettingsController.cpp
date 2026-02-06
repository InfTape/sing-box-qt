#include "views/settings/SettingsController.h"

#include "services/kernel/KernelManager.h"
#include "services/settings/SettingsService.h"

SettingsController::SettingsController(QObject* parent) : QObject(parent), m_kernelManager(new KernelManager(this)) {
  connect(m_kernelManager, &KernelManager::installedInfoReady, this, &SettingsController::installedInfoReady);
  connect(m_kernelManager, &KernelManager::releasesReady, this, &SettingsController::releasesReady);
  connect(m_kernelManager, &KernelManager::latestReady, this, &SettingsController::latestReady);
  connect(m_kernelManager, &KernelManager::downloadProgress, this, &SettingsController::downloadProgress);
  connect(m_kernelManager, &KernelManager::statusChanged, this, &SettingsController::statusChanged);
  connect(m_kernelManager, &KernelManager::finished, this, &SettingsController::finished);
}

SettingsModel::Data SettingsController::loadSettings() const {
  return SettingsService::loadSettings();
}

bool SettingsController::saveSettings(const SettingsModel::Data& data, int themeIndex, int languageIndex,
                                      QString* errorMessage) const {
  return SettingsService::saveSettings(data, themeIndex, languageIndex, errorMessage);
}

void SettingsController::refreshInstalledInfo() {
  if (m_kernelManager) m_kernelManager->refreshInstalledInfo();
}

void SettingsController::fetchReleaseList() {
  if (m_kernelManager) m_kernelManager->fetchReleaseList();
}

void SettingsController::checkLatest() {
  if (m_kernelManager) m_kernelManager->checkLatest();
}

void SettingsController::downloadAndInstall(const QString& version) {
  if (m_kernelManager) m_kernelManager->downloadAndInstall(version);
}
