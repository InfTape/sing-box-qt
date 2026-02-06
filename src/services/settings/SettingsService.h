#ifndef SETTINGSSERVICE_H
#define SETTINGSSERVICE_H
#include <QString>
#include "models/SettingsModel.h"

class SettingsService {
 public:
  static SettingsModel::Data loadSettings();
  static bool                saveSettings(const SettingsModel::Data& data, int themeIndex, int languageIndex,
                                          QString* errorMessage = nullptr);
};
#endif  // SETTINGSSERVICE_H
