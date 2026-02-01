#ifndef SETTINGSSERVICE_H
#define SETTINGSSERVICE_H

#include "models/SettingsModel.h"
#include <QString>

class SettingsService
{
public:
    static SettingsModel::Data loadSettings();
    static bool saveSettings(const SettingsModel::Data &data,
                             int themeIndex,
                             int languageIndex,
                             QString *errorMessage = nullptr);
};

#endif // SETTINGSSERVICE_H
