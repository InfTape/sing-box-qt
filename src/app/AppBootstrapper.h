#ifndef APPBOOTSTRAPPER_H
#define APPBOOTSTRAPPER_H

#include <memory>
#include <QTranslator>

class QApplication;
class MainWindow;
class TrayIcon;
class AppContext;

/**
 * @brief AppBootstrapper 负责应用级初始化（基础设施、翻译、上下文组装与 UI 创建）。
 */
class AppBootstrapper
{
public:
    explicit AppBootstrapper(QApplication &app);
    ~AppBootstrapper();

    bool initialize();
    bool createUI();
    void showMainWindow(bool startHidden);

    MainWindow* mainWindow() const;
    AppContext* context() const;

private:
    void setupMeta();
    void setupStyle();
    void setupFont();
    bool setupDatabase();
    void setupTheme();
    void loadTranslations();

    QApplication &m_app;
    QTranslator m_translator;
    std::unique_ptr<AppContext> m_context;
    std::unique_ptr<MainWindow> m_mainWindow;
    std::unique_ptr<TrayIcon> m_trayIcon;
};

#endif // APPBOOTSTRAPPER_H
