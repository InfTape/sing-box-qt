#include "ThemeManager.h"
#include <QApplication>
#include <QFile>
#include <QFont>
#include <QJsonObject>
#include <QPalette>
#include <QSettings>
#include "storage/DatabaseService.h"
#include "utils/Logger.h"

ThemeManager& ThemeManager::instance() {
  static ThemeManager instance;
  return instance;
}

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent),
      m_currentMode(Dark)  // Default to dark.
{}

ThemeManager::~ThemeManager() {}

void ThemeManager::init() {
  // Load theme settings from database.
  QJsonObject themeConfig = DatabaseService::instance().getThemeConfig();
  QString     themeEntry  = themeConfig.value("theme").toString("dark");
  if (themeEntry == "light") {
    m_currentMode = Light;
  } else if (themeEntry == "auto") {
    m_currentMode = Auto;
    // Actual palette will follow the system theme inside loadThemeColors().
  } else {
    m_currentMode = Dark;
  }
  loadThemeColors();
  updateApplicationStyle();
}

void ThemeManager::setThemeMode(ThemeMode mode) {
  m_currentMode = mode;
  loadThemeColors();
  updateApplicationStyle();
  // Save settings.
  QJsonObject config = DatabaseService::instance().getThemeConfig();
  config["theme"]    = (mode == Light ? "light" : (mode == Auto ? "auto" : "dark"));
  DatabaseService::instance().saveThemeConfig(config);
  emit themeChanged();
}

ThemeManager::ThemeMode ThemeManager::getThemeMode() const {
  return m_currentMode;
}

void ThemeManager::loadThemeColors() {
  m_colors.clear();
  // Base palette (Tailwind CSS style).
  m_colors["primary"]        = "#5aa9ff";  // Light Blue
  m_colors["primary-hover"]  = "#7bbcff";  // Light Blue Hover
  m_colors["primary-active"] = "#3f8ff2";  // Light Blue Active
  m_colors["success"]        = "#10b981";  // Emerald 500
  m_colors["warning"]        = "#f59e0b";  // Amber 500
  m_colors["error"]          = "#ef4444";  // Red 500
  auto addAlpha              = [this](const QString& key, const QColor& color, double alpha) {
    // 使用 rgba() 格式而不是 HexArgb，因为 Qt 样式表中 border 属性对 HexArgb 支持有问题
    m_colors[key] = QString("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(alpha);
  };
  // Derived hover colors
  {
    QColor err(m_colors["error"]);
    err                     = err.lighter(110);
    m_colors["error-hover"] = err.name();
  }
  const ThemeMode effectiveMode = resolveModeForColors();
  if (effectiveMode == Light) {
    // Light theme variables.
    m_colors["bg-primary"]     = "#f8fafc";  // Slate 50
    m_colors["bg-secondary"]   = "#ffffff";  // White
    m_colors["bg-tertiary"]    = "#f1f5f9";  // Slate 100
    m_colors["text-primary"]   = "#0f172a";  // Slate 900
    m_colors["text-secondary"] = "#475569";  // Slate 600
    m_colors["text-tertiary"]  = "#94a3b8";  // Slate 400
    m_colors["border"]         = "rgba(148, 163, 184, 0.2)";
    m_colors["border-hover"]   = "rgba(90, 169, 255, 0.35)";
    m_colors["border-solid"]   = "#d1d8de";  // Light border for solids
    m_colors["panel-bg"]       = "#ffffff";
    m_colors["input-bg"]       = "#f1f5f9";
  } else {
    // Dark theme variables.
    m_colors["bg-primary"]     = "#0f172a";  // Slate 900
    m_colors["bg-secondary"]   = "#1e293b";  // Slate 800
    m_colors["bg-tertiary"]    = "#334155";  // Slate 700
    m_colors["text-primary"]   = "#f8fafc";  // Slate 50
    m_colors["text-secondary"] = "#cbd5e1";  // Slate 300
    m_colors["text-tertiary"]  = "#64748b";  // Slate 500
    m_colors["border"]         = "rgba(255, 255, 255, 0.1)";
    m_colors["border-hover"]   = "rgba(90, 169, 255, 0.45)";
    m_colors["border-solid"]   = "#3f454d";  // Keep original for dark mode
    m_colors["panel-bg"]       = "#1e293b";  // Slate 800
    m_colors["input-bg"]       = "#0f172a";  // Slate 900
  }
  // Derived translucent colors (for backgrounds/overlays).
  addAlpha("primary-06", QColor(m_colors["primary"]), 0.06);
  addAlpha("primary-12", QColor(m_colors["primary"]), 0.12);
  addAlpha("primary-18", QColor(m_colors["primary"]), 0.18);
  addAlpha("primary-20", QColor(m_colors["primary"]), 0.20);
  addAlpha("primary-30", QColor(m_colors["primary"]), 0.30);
  addAlpha("primary-40", QColor(m_colors["primary"]), 0.40);
  addAlpha("success-12", QColor(m_colors["success"]), 0.12);
  addAlpha("success-18", QColor(m_colors["success"]), 0.18);
  addAlpha("success-20", QColor(m_colors["success"]), 0.20);
  addAlpha("success-30", QColor(m_colors["success"]), 0.30);
  addAlpha("success-40", QColor(m_colors["success"]), 0.40);
  addAlpha("warning-12", QColor(m_colors["warning"]), 0.12);
  addAlpha("warning-18", QColor(m_colors["warning"]), 0.18);
  addAlpha("warning-20", QColor(m_colors["warning"]), 0.20);
  addAlpha("warning-30", QColor(m_colors["warning"]), 0.30);
  addAlpha("warning-40", QColor(m_colors["warning"]), 0.40);
  addAlpha("error-12", QColor(m_colors["error"]), 0.12);
  addAlpha("error-18", QColor(m_colors["error"]), 0.18);
  addAlpha("error-20", QColor(m_colors["error"]), 0.20);
  addAlpha("error-30", QColor(m_colors["error"]), 0.30);
  addAlpha("error-40", QColor(m_colors["error"]), 0.40);
}

ThemeManager::ThemeMode ThemeManager::resolveModeForColors() const {
  if (m_currentMode != Auto) {
    return m_currentMode;
  }
#if defined(Q_OS_WIN)
  QSettings settings(
      "HKEY_CURRENT_"
      "USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      QSettings::NativeFormat);
  const bool useLight = settings.value("AppsUseLightTheme", 0).toInt() != 0;
  return useLight ? Light : Dark;
#else
  return Dark;
#endif
}

QColor ThemeManager::getColor(const QString& key) const {
  return QColor(m_colors.value(key, "#000000"));
}

QString ThemeManager::getColorString(const QString& key) const {
  return m_colors.value(key, "#000000");
}

QString ThemeManager::getGlobalStyleSheet() const {
  QString family = qApp->property("appFontFamily").toString();
  if (family.isEmpty()) {
    family = qApp->font().family();
  }
  QString familyList = qApp->property("appFontFamilyList").toString();
  if (familyList.isEmpty()) {
    familyList = family;
  }
  if (!familyList.startsWith("'")) {
    familyList.prepend("'");
  }
  if (!familyList.endsWith("'")) {
    familyList.append("'");
  }
  QMap<QString, QString> extra;
  extra.insert("font-family", family);
  extra.insert("font-family-list", familyList);
  return loadStyleSheet(":/styles/global.qss", extra);
}

QString ThemeManager::getLogViewStyle() const {
  return loadStyleSheet(":/styles/log_view.qss");
}

QString ThemeManager::loadStyleSheet(const QString& resourcePath, const QMap<QString, QString>& extra) const {
  QFile file(resourcePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    Logger::warn(QString("Failed to open stylesheet: %1").arg(resourcePath));
    return QString();
  }
  QString qss = QString::fromUtf8(file.readAll());
  file.close();
  for (auto it = m_colors.constBegin(); it != m_colors.constEnd(); ++it) {
    qss.replace(QString("@%1@").arg(it.key()), it.value());
  }
  for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
    qss.replace(QString("@%1@").arg(it.key()), it.value());
  }
  return qss;
}

void ThemeManager::updateApplicationStyle() {
  const QString family = qApp->property("appFontFamily").toString();
  if (!family.isEmpty()) {
    QFont font = qApp->font();
    font.setFamily(family);
    QStringList families = qApp->property("appFontFamilyList").toString().split("','", Qt::SkipEmptyParts);
    if (!families.isEmpty()) {
      font.setFamilies(families);
    }
    qApp->setFont(font);
  }
  qApp->setStyleSheet(getGlobalStyleSheet());
  // Set Qt palette for widgets not fully covered by styles.
  QPalette palette;
  palette.setColor(QPalette::Window, getColor("bg-primary"));
  palette.setColor(QPalette::WindowText, getColor("text-primary"));
  palette.setColor(QPalette::Base, getColor("bg-secondary"));
  palette.setColor(QPalette::AlternateBase, getColor("bg-tertiary"));
  palette.setColor(QPalette::ToolTipBase, getColor("bg-secondary"));
  palette.setColor(QPalette::ToolTipText, getColor("text-primary"));
  palette.setColor(QPalette::Text, getColor("text-primary"));
  palette.setColor(QPalette::Button, getColor("bg-secondary"));
  palette.setColor(QPalette::ButtonText, getColor("text-primary"));
  palette.setColor(QPalette::BrightText, Qt::red);
  palette.setColor(QPalette::Link, getColor("primary"));
  palette.setColor(QPalette::Highlight, getColor("primary"));
  palette.setColor(QPalette::HighlightedText, Qt::white);
  qApp->setPalette(palette);
}
