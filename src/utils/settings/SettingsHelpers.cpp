#include "utils/settings/SettingsHelpers.h"
#include <QLineEdit>
#include <QRegularExpression>

namespace SettingsHelpers {
int themeIndexFromMode(ThemeService::ThemeMode mode) {
  switch (mode) {
    case ThemeService::ThemeMode::Light:
      return 1;
    case ThemeService::ThemeMode::Auto:
      return 2;
    case ThemeService::ThemeMode::Dark:
    default:
      return 0;
  }
}

ThemeService::ThemeMode themeModeFromIndex(int index) {
  switch (index) {
    case 1:
      return ThemeService::ThemeMode::Light;
    case 2:
      return ThemeService::ThemeMode::Auto;
    default:
      return ThemeService::ThemeMode::Dark;
  }
}

QString normalizeBypassText(const QString& text) {
  QString bypass = text;
  bypass.replace(QRegularExpression("[\\r\\n]+"), ";");
  return bypass.trimmed();
}

QString resolveTextOrDefault(const QLineEdit* edit, const QString& fallback) {
  if (!edit) {
    return fallback;
  }
  const QString value = edit->text().trimmed();
  return value.isEmpty() ? fallback : value;
}
}  // namespace SettingsHelpers
