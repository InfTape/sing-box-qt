#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H
#include <QColor>
#include <QMap>
#include <QObject>
#include <QString>

class ThemeManager : public QObject {
  Q_OBJECT
 public:
  enum ThemeMode { Light, Dark, Auto };

  static ThemeManager& instance();
  void                 init();
  void                 setThemeMode(ThemeMode mode);
  ThemeMode            getThemeMode() const;
  // Get colors.
  QColor  getColor(const QString& key) const;
  QString getColorString(const QString& key) const;
  // Get global stylesheet.
  QString getGlobalStyleSheet() const;
  QString getLogViewStyle() const;
  QString loadStyleSheet(
      const QString&                resourcePath,
      const QMap<QString, QString>& extra = QMap<QString, QString>()) const;
 signals:
  void themeChanged();

 private:
  ThemeManager(QObject* parent = nullptr);
  ~ThemeManager();
  void                   loadThemeColors();
  void                   updateApplicationStyle();
  ThemeMode              resolveModeForColors() const;
  ThemeMode              m_currentMode;
  QMap<QString, QString> m_colors;
};
#endif  // THEMEMANAGER_H
