#include "SettingsService.h"
#include "storage/DatabaseService.h"
#include "system/AutoStart.h"
#include "utils/Logger.h"
#include <QJsonObject>
#include <QObject>

SettingsModel::Data SettingsService::loadSettings()
{
    SettingsModel::Data data = SettingsModel::load();

    // 同步系统自启动状态，确保显示与实际一致。
    if (AutoStart::isSupported()) {
        if (data.autoStart != AutoStart::isEnabled()) {
            AutoStart::setEnabled(data.autoStart);
        }
        data.autoStart = AutoStart::isEnabled();
    }

    return data;
}

bool SettingsService::saveSettings(const SettingsModel::Data &data,
                                   int themeIndex,
                                   int languageIndex,
                                   QString *errorMessage)
{
    SettingsModel::Data mutableData = data;

    // 处理自启动（系统副作用集中于此）。
    if (AutoStart::isSupported()) {
        if (!AutoStart::setEnabled(mutableData.autoStart)) {
            mutableData.autoStart = AutoStart::isEnabled();
            if (errorMessage) {
                *errorMessage = QObject::tr("Failed to set auto-start");
            }
            return false;
        }
        mutableData.autoStart = AutoStart::isEnabled();
    }

    // 保存通用设置。
    if (!SettingsModel::save(mutableData, errorMessage)) {
        return false;
    }

    // 主题保存。
    QJsonObject theme;
    switch (themeIndex) {
        case 1: theme["theme"] = "light"; break;
        case 2: theme["theme"] = "auto"; break;
        default: theme["theme"] = "dark"; break;
    }
    DatabaseService::instance().saveThemeConfig(theme);

    // 语言保存。
    const QString locales[] = {"zh_CN", "en", "ja", "ru"};
    const int safeIndex = (languageIndex >= 0 && languageIndex < 4) ? languageIndex : 0;
    DatabaseService::instance().saveLocale(locales[safeIndex]);

    Logger::info(QObject::tr("Settings saved"));
    return true;
}
