#ifndef THEMESERVICEADAPTER_H
#define THEMESERVICEADAPTER_H

#include "app/interfaces/ThemeService.h"
class ThemeServiceAdapter : public ThemeService {
  Q_OBJECT
 public:
  explicit ThemeServiceAdapter(QObject* parent = nullptr);

  void      init() override;
  QColor    color(const QString& key) const override;
  QString   colorString(const QString& key) const override;
  QString   loadStyleSheet(const QString& resourcePath, const QMap<QString, QString>& extra = {}) const override;
  ThemeMode themeMode() const override;
  void      setThemeMode(ThemeMode mode) override;
};
#endif  // THEMESERVICEADAPTER_H
