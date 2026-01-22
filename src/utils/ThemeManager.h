#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QString>
#include <QColor>
#include <QMap>

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    enum ThemeMode {
        Light,
        Dark,
        Auto
    };

    static ThemeManager& instance();
    
    void init();
    void setThemeMode(ThemeMode mode);
    ThemeMode getThemeMode() const;
    
    // 获取颜色
    QColor getColor(const QString &key) const;
    QString getColorString(const QString &key) const;
    
    // 获取全局样式表
    QString getGlobalStyleSheet() const;
    
    // 获取特定组件样式
    QString getButtonStyle() const;
    QString getCardStyle() const;
    QString getInputStyle() const;
    QString getScrollBarStyle() const;

signals:
    void themeChanged();

private:
    ThemeManager(QObject *parent = nullptr);
    ~ThemeManager();
    
    void loadThemeColors();
    void updateApplicationStyle();
    
    ThemeMode m_currentMode;
    QMap<QString, QString> m_colors;
};

#endif // THEMEMANAGER_H
