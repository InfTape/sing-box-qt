#include "services/settings/SettingsService.h"
#include <QJsonObject>
#include <QObject>
#include "storage/DatabaseService.h"
#include "system/AutoStart.h"
#include "utils/Logger.h"

SettingsModel::Data SettingsService::loadSettings() {
  SettingsModel::Data data = SettingsModel::load();
  // Sync system auto-start status, ensuring display matches reality.
  if (AutoStart::isSupported()) {
    if (data.autoStart != AutoStart::isEnabled()) {
      AutoStart::setEnabled(data.autoStart);
    }
    data.autoStart = AutoStart::isEnabled();
  }
  return data;
}

bool SettingsService::saveSettings(const SettingsModel::Data& data, int themeIndex, int languageIndex,
                                   QString* errorMessage) {
  SettingsModel::Data mutableData = data;
  // Handle auto-start (system side effects concentrated here).
  if (AutoStart::isSupported()) {
    if (!AutoStart::setEnabled(mutableData.autoStart)) {
      mutableData.autoStart = AutoStart::isEnabled();
      if (errorMessage) {
        *errorMessage = QObject::tr("Failed to set auto-start");
      }
      return false;
    }
    mutableData.autoStart = AutoStart::isEnabled();
  }
  // Save general settings.
  if (!SettingsModel::save(mutableData, errorMessage)) {
    return false;
  }
  // Save theme.
  QJsonObject theme;
  switch (themeIndex) {
    case 1:
      theme["theme"] = "light";
      break;
    case 2:
      theme["theme"] = "auto";
      break;
    default:
      theme["theme"] = "dark";
      break;
  }
  DatabaseService::instance().saveThemeConfig(theme);
  // Save language.
  const QString locales[] = {"zh_CN", "en", "ja", "ru"};
  const int     safeIndex = (languageIndex >= 0 && languageIndex < 4) ? languageIndex : 0;
  DatabaseService::instance().saveLocale(locales[safeIndex]);
  Logger::info(QObject::tr("Settings saved"));
  return true;
}
