#ifndef THEMESERVICE_H
#define THEMESERVICE_H
#include <QColor>
#include <QMap>
#include <QObject>
#include <QString>

class ThemeService : public QObject {
  Q_OBJECT
 public:
  enum class ThemeMode { Dark, Light, Auto };

  explicit ThemeService(QObject* parent = nullptr) : QObject(parent) {}

  ~ThemeService() override                                = default;
  virtual void      init()                                = 0;
  virtual QColor    color(const QString& key) const       = 0;
  virtual QString   colorString(const QString& key) const = 0;
  virtual QString   loadStyleSheet(const QString& resourcePath, const QMap<QString, QString>& extra = {}) const = 0;
  virtual ThemeMode themeMode() const                                                                           = 0;
  virtual void      setThemeMode(ThemeMode mode)                                                                = 0;
 signals:
  void themeChanged();
};
#endif  // THEMESERVICE_H
