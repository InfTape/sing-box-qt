#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QStyleFactory>
#include <QIcon>

#include <QFont>
#include "app/MainWindow.h"
#include "app/TrayIcon.h"
#include "utils/Logger.h"
#include "storage/DatabaseService.h"
#include "utils/ThemeManager.h"

int main(int argc, char *argv[])
{
    // 启用高 DPI 支持
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    
    // 应用信息
    app.setApplicationName("Sing-Box");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("XingGao");
    app.setOrganizationDomain("github.com/xinggaoya");
    app.setWindowIcon(QIcon(":/icons/app.png"));
    
    // 设置应用样式
    app.setStyle(QStyleFactory::create("Fusion"));
    
    // 初始化日志系统
    Logger::instance().init();
    Logger::info("应用启动中...");
    
    // 设置默认字体为微软雅黑
    QStringList families;
    families << "Microsoft YaHei"
             << "Microsoft YaHei UI"
             << "Segoe UI"
             << "PingFang SC"
             << "Noto Sans SC";

    QFont defaultFont("Microsoft YaHei", 10);
    defaultFont.setStyleStrategy(QFont::PreferAntialias);
    defaultFont.setFamilies(families);
    app.setFont(defaultFont);

    app.setProperty("appFontFamily", "Microsoft YaHei");
    app.setProperty("appFontFamilyList", families.join("','"));
    Logger::info("已设置默认字体: Microsoft YaHei");
    
    // 初始化数据库
    if (!DatabaseService::instance().init()) {
        Logger::error("数据库初始化失败");
        return -1;
    }

    // 初始化主题管理器
    ThemeManager::instance().init();
    
    // 加载翻译
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "sing-box-qt_" + QLocale(locale).name();
        if (translator.load(":/translations/" + baseName)) {
            app.installTranslator(&translator);
            break;
        }
    }
    
    // 检查启动参数
    bool startHidden = app.arguments().contains("--hide");
    
    // 创建主窗口
    MainWindow mainWindow;
    mainWindow.setWindowIcon(QIcon(":/icons/app.png"));
    
    // 创建系统托盘
    TrayIcon trayIcon(&mainWindow);
    trayIcon.show();
    
    // 根据参数决定是否显示窗口
    if (!startHidden) {
        mainWindow.show();
    }
    
    Logger::info("应用启动完成");
    
    return app.exec();
}
