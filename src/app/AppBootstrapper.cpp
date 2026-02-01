#include "AppBootstrapper.h"

#include <QApplication>
#include <QStyleFactory>
#include <QIcon>
#include <QFont>
#include <QLocale>
#include <QStringList>

#include "app/AppContext.h"
#include "app/MainWindow.h"
#include "app/TrayIcon.h"
#include "utils/Logger.h"
#include "storage/DatabaseService.h"
#include "utils/ThemeManager.h"

AppBootstrapper::AppBootstrapper(QApplication &app)
    : m_app(app)
{
}

AppBootstrapper::~AppBootstrapper() = default;

bool AppBootstrapper::initialize()
{
    setupMeta();
    setupStyle();

    Logger::instance().init();
    Logger::info("Application is starting...");

    setupFont();
    if (!setupDatabase()) {
        return false;
    }

    if (!m_context) {
        m_context = std::make_unique<AppContext>();
    }

    if (m_context->themeService()) {
        m_context->themeService()->init();
    } else {
        setupTheme();
    }
    loadTranslations();

    return true;
}

bool AppBootstrapper::createUI()
{
    if (!m_context && !initialize()) {
        return false;
    }

    m_mainWindow = std::make_unique<MainWindow>(*m_context);
    m_mainWindow->setWindowIcon(QIcon(":/icons/app.png"));

    m_trayIcon = std::make_unique<TrayIcon>(m_mainWindow.get(), m_context ? m_context->themeService() : nullptr);
    m_trayIcon->show();

    Logger::info("Application initialized, UI ready");
    return true;
}

void AppBootstrapper::showMainWindow(bool startHidden)
{
    if (!m_mainWindow) return;
    if (!startHidden) {
        m_mainWindow->show();
    }
    Logger::info("Application started");
}

MainWindow* AppBootstrapper::mainWindow() const
{
    return m_mainWindow.get();
}

AppContext* AppBootstrapper::context() const
{
    return m_context.get();
}

void AppBootstrapper::setupMeta()
{
    m_app.setApplicationName("Sing-Box——Qt");
    m_app.setApplicationVersion("1.0.5");
    m_app.setOrganizationName("InfTape");
    m_app.setOrganizationDomain("github.com/inftape");
    m_app.setWindowIcon(QIcon(":/icons/app.png"));
}

void AppBootstrapper::setupStyle()
{
    m_app.setStyle(QStyleFactory::create("Fusion"));
}

void AppBootstrapper::setupFont()
{
    QStringList families;
    families << "Microsoft YaHei"
             << "Microsoft YaHei UI"
             << "Segoe UI"
             << "PingFang SC"
             << "Noto Sans SC";

    QFont defaultFont("Microsoft YaHei", 10);
    defaultFont.setStyleStrategy(QFont::PreferAntialias);
    defaultFont.setFamilies(families);
    m_app.setFont(defaultFont);

    m_app.setProperty("appFontFamily", "Microsoft YaHei");
    m_app.setProperty("appFontFamilyList", families.join("','"));
    Logger::info("Default font set: Microsoft YaHei");
}

bool AppBootstrapper::setupDatabase()
{
    if (!DatabaseService::instance().init()) {
        Logger::error("Database initialization failed");
        return false;
    }
    return true;
}

void AppBootstrapper::setupTheme()
{
    ThemeManager::instance().init();
}

void AppBootstrapper::loadTranslations()
{
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "sing-box-qt_" + QLocale(locale).name();
        if (m_translator.load(":/translations/" + baseName)) {
            m_app.installTranslator(&m_translator);
            break;
        }
    }
}
