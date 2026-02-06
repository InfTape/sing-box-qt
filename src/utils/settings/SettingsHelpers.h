#pragma once
#include <QString>
#include "app/interfaces/ThemeService.h"
class QLineEdit;
namespace SettingsHelpers {
int                     themeIndexFromMode(ThemeService::ThemeMode mode);
ThemeService::ThemeMode themeModeFromIndex(int index);
QString                 normalizeBypassText(const QString& text);
QString                 resolveTextOrDefault(const QLineEdit* edit, const QString& fallback);
}  // namespace SettingsHelpers
