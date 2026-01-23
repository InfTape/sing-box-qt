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
    // Enable high DPI support
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);

    // Application metadata
    app.setApplicationName("Sing-Box");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("XingGao");
    app.setOrganizationDomain("github.com/xinggaoya");
    app.setWindowIcon(QIcon(":/icons/app.png"));

    // Set application style
    app.setStyle(QStyleFactory::create("Fusion"));

    // Initialize logging
    Logger::instance().init();
    Logger::info("Application is starting...");

    // Set default font family
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
    Logger::info("Default font set: Microsoft YaHei");

    // Initialize database
    if (!DatabaseService::instance().init()) {
        Logger::error("Database initialization failed");
        return -1;
    }

    // Initialize theme manager
    ThemeManager::instance().init();

    // Load translations
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "sing-box-qt_" + QLocale(locale).name();
        if (translator.load(":/translations/" + baseName)) {
            app.installTranslator(&translator);
            break;
        }
    }

    // Check startup arguments
    bool startHidden = app.arguments().contains("--hide");

    // Create main window
    MainWindow mainWindow;
    mainWindow.setWindowIcon(QIcon(":/icons/app.png"));

    // Create system tray icon
    TrayIcon trayIcon(&mainWindow);
    trayIcon.show();

    // Show window unless started hidden
    if (!startHidden) {
        mainWindow.show();
    }

    Logger::info("Application started");

    return app.exec();
}
