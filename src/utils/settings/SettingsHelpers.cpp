#include "utils/settings/SettingsHelpers.h"

#include <QLineEdit>
#include <QRegularExpression>
namespace SettingsHelpers {
int themeIndexFromMode(ThemeManager::ThemeMode mode) {
  switch (mode) {
    case ThemeManager::Light:
      return 1;
    case ThemeManager::Auto:
      return 2;
    default:
      return 0;
  }
}
ThemeManager::ThemeMode themeModeFromIndex(int index) {
  switch (index) {
    case 1:
      return ThemeManager::Light;
    case 2:
      return ThemeManager::Auto;
    default:
      return ThemeManager::Dark;
  }
}
QString normalizeBypassText(const QString& text) {
  QString bypass = text;
  bypass.replace(QRegularExpression("[\\r\\n]+"), ";");
  return bypass.trimmed();
}
QString resolveTextOrDefault(const QLineEdit* edit, const QString& fallback) {
  if (!edit) return fallback;
  const QString value = edit->text().trimmed();
  return value.isEmpty() ? fallback : value;
}
}  // namespace SettingsHelpers
