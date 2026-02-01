#include "ThemeServiceAdapter.h"

#include "utils/ThemeManager.h"

namespace {
ThemeManager::ThemeMode toThemeManagerMode(ThemeService::ThemeMode mode) {
  switch (mode) {
    case ThemeService::ThemeMode::Light:
      return ThemeManager::Light;
    case ThemeService::ThemeMode::Auto:
      return ThemeManager::Auto;
    case ThemeService::ThemeMode::Dark:
    default:
      return ThemeManager::Dark;
  }
}

ThemeService::ThemeMode toServiceMode(ThemeManager::ThemeMode mode) {
  switch (mode) {
    case ThemeManager::Light:
      return ThemeService::ThemeMode::Light;
    case ThemeManager::Auto:
      return ThemeService::ThemeMode::Auto;
    case ThemeManager::Dark:
    default:
      return ThemeService::ThemeMode::Dark;
  }
}
}  // namespace

ThemeServiceAdapter::ThemeServiceAdapter(QObject* parent)
    : ThemeService(parent) {
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
          &ThemeService::themeChanged);
}

void ThemeServiceAdapter::init() { ThemeManager::instance().init(); }

QColor ThemeServiceAdapter::color(const QString& key) const {
  return ThemeManager::instance().getColor(key);
}

QString ThemeServiceAdapter::colorString(const QString& key) const {
  return ThemeManager::instance().getColorString(key);
}

QString ThemeServiceAdapter::loadStyleSheet(
    const QString& resourcePath, const QMap<QString, QString>& extra) const {
  return ThemeManager::instance().loadStyleSheet(resourcePath, extra);
}

ThemeService::ThemeMode ThemeServiceAdapter::themeMode() const {
  return toServiceMode(ThemeManager::instance().getThemeMode());
}

void ThemeServiceAdapter::setThemeMode(ThemeMode mode) {
  ThemeManager::instance().setThemeMode(toThemeManagerMode(mode));
}
