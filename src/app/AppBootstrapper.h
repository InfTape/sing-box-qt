#ifndef APPBOOTSTRAPPER_H
#define APPBOOTSTRAPPER_H
#include <QTranslator>
#include <memory>
class QApplication;
class MainWindow;
class TrayIcon;
class AppContext;
/**
 * @brief AppBootstrapper (Infrastructure, Translation, Context assembly, and UI
 * Creation).
 */
class AppBootstrapper {
 public:
  explicit AppBootstrapper(QApplication& app);
  ~AppBootstrapper();
  bool        initialize();
  bool        createUI();
  void        showMainWindow(bool startHidden);
  MainWindow* mainWindow() const;
  AppContext* context() const;

 private:
  void                        setupMeta();
  void                        setupStyle();
  void                        setupFont();
  bool                        setupDatabase();
  void                        loadTranslations();
  QApplication&               m_app;
  QTranslator                 m_translator;
  std::unique_ptr<AppContext> m_context;
  std::unique_ptr<MainWindow> m_mainWindow;
  std::unique_ptr<TrayIcon>   m_trayIcon;
};
#endif  // APPBOOTSTRAPPER_H
