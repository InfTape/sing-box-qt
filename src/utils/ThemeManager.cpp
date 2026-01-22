#include "ThemeManager.h"
#include "storage/DatabaseService.h"
#include "utils/Logger.h"
#include <QApplication>
#include <QPalette>
#include <QJsonObject>
#include <QStyleFactory>
#include <QFont>

ThemeManager& ThemeManager::instance()
{
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
    , m_currentMode(Dark) // 默认为深色
{
}

ThemeManager::~ThemeManager()
{
}

void ThemeManager::init()
{
    // 从数据库加载主题设置
    QJsonObject themeConfig = DatabaseService::instance().getThemeConfig();
    QString themeEntry = themeConfig.value("theme").toString("dark");
    
    if (themeEntry == "light") {
        m_currentMode = Light;
    } else if (themeEntry == "auto") {
        m_currentMode = Auto;
        // TODO: 检测系统主题
    } else {
        m_currentMode = Dark;
    }
    
    loadThemeColors();
    updateApplicationStyle();
}

void ThemeManager::setThemeMode(ThemeMode mode)
{
    if (m_currentMode == mode) return;
    
    m_currentMode = mode;
    loadThemeColors();
    updateApplicationStyle();
    
    // 保存设置
    QJsonObject config = DatabaseService::instance().getThemeConfig();
    config["theme"] = (mode == Light ? "light" : (mode == Auto ? "auto" : "dark"));
    DatabaseService::instance().saveThemeConfig(config);
    
    emit themeChanged();
}

ThemeManager::ThemeMode ThemeManager::getThemeMode() const
{
    return m_currentMode;
}

void ThemeManager::loadThemeColors()
{
    m_colors.clear();
    
    // 基础调色板 (Tailwind CSS 风格)
    m_colors["primary"] = "#5aa9ff";       // Light Blue
    m_colors["primary-hover"] = "#7bbcff"; // Light Blue Hover
    m_colors["primary-active"] = "#3f8ff2"; // Light Blue Active
    
    m_colors["success"] = "#10b981";       // Emerald 500
    m_colors["warning"] = "#f59e0b";       // Amber 500
    m_colors["error"] = "#ef4444";         // Red 500
    
    if (m_currentMode == Light) {
        // 浅色主题变量
        m_colors["bg-primary"] = "#f8fafc";   // Slate 50
        m_colors["bg-secondary"] = "#ffffff"; // White
        m_colors["bg-tertiary"] = "#f1f5f9";  // Slate 100
        
        m_colors["text-primary"] = "#0f172a";   // Slate 900
        m_colors["text-secondary"] = "#475569"; // Slate 600
        m_colors["text-tertiary"] = "#94a3b8";  // Slate 400
        
        m_colors["border"] = "rgba(148, 163, 184, 0.2)";
        m_colors["border-hover"] = "rgba(90, 169, 255, 0.35)";
        
        m_colors["panel-bg"] = "#ffffff";
        m_colors["input-bg"] = "#f1f5f9";
    } else {
        // 深色主题变量
        m_colors["bg-primary"] = "#0f172a";   // Slate 900
        m_colors["bg-secondary"] = "#1e293b"; // Slate 800
        m_colors["bg-tertiary"] = "#334155";  // Slate 700
        
        m_colors["text-primary"] = "#f8fafc";   // Slate 50
        m_colors["text-secondary"] = "#cbd5e1"; // Slate 300
        m_colors["text-tertiary"] = "#64748b";  // Slate 500
        
        m_colors["border"] = "rgba(255, 255, 255, 0.1)";
        m_colors["border-hover"] = "rgba(90, 169, 255, 0.45)";
        
        m_colors["panel-bg"] = "#1e293b"; // Slate 800
        m_colors["input-bg"] = "#0f172a"; // Slate 900
    }
}

QColor ThemeManager::getColor(const QString &key) const
{
    return QColor(m_colors.value(key, "#000000"));
}

QString ThemeManager::getColorString(const QString &key) const
{
    return m_colors.value(key, "#000000");
}

QString ThemeManager::getGlobalStyleSheet() const
{
    QString family = qApp->property("appFontFamily").toString();
    if (family.isEmpty()) {
        family = qApp->font().family();
    }
    QString familyList = qApp->property("appFontFamilyList").toString();
    if (familyList.isEmpty()) {
        familyList = family;
    }
    return QString(R"(
        QMainWindow, QDialog {
            background-color: %1;
            color: %2;
        }
        QWidget {
            font-family: '%3';
            font-size: 14px;
        }
        QLabel {
            color: %2;
        }
    )")
    .arg(m_colors["bg-primary"])
    .arg(m_colors["text-primary"])
    .arg(familyList);
}

QString ThemeManager::getButtonStyle() const
{
    return QString(R"(
        QPushButton {
            background-color: %1;
            color: white;
            border: none;
            padding: 8px 16px;
            border-radius: 10px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: %2;
        }
        QPushButton:pressed {
            background-color: %3;
        }
        QPushButton:disabled {
            background-color: %4;
            color: %5;
        }
    )")
    .arg(m_colors["primary"])
    .arg(m_colors["primary-hover"])
    .arg(m_colors["primary-active"])
    .arg(m_colors["bg-tertiary"])
    .arg(m_colors["text-tertiary"]);
}

QString ThemeManager::getCardStyle() const
{
    return QString(R"(
        QFrame, QWidget#Card {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 10px;
        }
    )")
    .arg(m_colors["panel-bg"])
    .arg(m_colors["border"]);
}

QString ThemeManager::getInputStyle() const
{
    return QString(R"(
        QLineEdit, QSpinBox, QComboBox, QPlainTextEdit {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 10px;
            padding: 8px 12px;
            color: %3;
            selection-background-color: %4;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QPlainTextEdit:focus {
            border: 1px solid %4;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox QAbstractItemView {
            background-color: %1;
            color: %3;
            selection-background-color: %4;
            border: 1px solid %2;
        }
    )")
    .arg(m_colors["input-bg"])
    .arg(m_colors["border"])
    .arg(m_colors["text-primary"])
    .arg(m_colors["primary"]);
}

QString ThemeManager::getScrollBarStyle() const
{
    return QString(R"(
        QScrollBar:vertical {
            border: none;
            background: transparent;
            width: 8px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: %1;
            min-height: 20px;
            border-radius: 10px;
        }
        QScrollBar::handle:vertical:hover {
            background: %2;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
    )")
    .arg(m_colors["border"])
    .arg(m_colors["text-tertiary"]);
}

void ThemeManager::updateApplicationStyle()
{
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
    qApp->setStyleSheet(getGlobalStyleSheet() + 
                       getButtonStyle() + 
                       getInputStyle() + 
                       getScrollBarStyle());
                       
    // 设置 Qt Palette 以兼容一些未完全覆盖的组件
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
