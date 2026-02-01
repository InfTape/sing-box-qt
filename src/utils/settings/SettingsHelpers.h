#pragma once

#include <QString>

#include "utils/ThemeManager.h"

class QLineEdit;
namespace SettingsHelpers {
int                     themeIndexFromMode(ThemeManager::ThemeMode mode);
ThemeManager::ThemeMode themeModeFromIndex(int index);
QString                 normalizeBypassText(const QString& text);
QString resolveTextOrDefault(const QLineEdit* edit, const QString& fallback);
}  // namespace SettingsHelpers
