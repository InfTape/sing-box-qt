#include "ThemeServiceAdapter.h"

#include "utils/ThemeManager.h"

ThemeServiceAdapter::ThemeServiceAdapter(QObject *parent)
    : ThemeService(parent)
{
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &ThemeService::themeChanged);
}

void ThemeServiceAdapter::init()
{
    ThemeManager::instance().init();
}

QColor ThemeServiceAdapter::color(const QString &key) const
{
    return ThemeManager::instance().getColor(key);
}

QString ThemeServiceAdapter::colorString(const QString &key) const
{
    return ThemeManager::instance().getColorString(key);
}

QString ThemeServiceAdapter::loadStyleSheet(const QString &resourcePath,
                                            const QMap<QString, QString> &extra) const
{
    return ThemeManager::instance().loadStyleSheet(resourcePath, extra);
}
