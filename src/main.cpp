#include <QApplication>
#include <QObject>
#include <QStringList>
#include "app/AppBootstrapper.h"
#include "app/MainWindow.h"
#include "system/SingleInstanceGuard.h"

namespace {
constexpr auto kInstanceKey                 = "InfTape.Sing-Box-Qt";
constexpr auto kReplaceExistingInstanceArg = "--replace-existing-instance";
}

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
  // Qt 6's DirectWrite engine only does grayscale antialiasing, which looks
  // fuzzy on standard-DPI displays; FreeType renders noticeably crisper.
  // Users can still override via QT_QPA_PLATFORM or -platform.
  if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", "windows:fontengine=freetype");
  }
#endif
  // Enable high DPI support
  QApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
  QApplication app(argc, argv);

  const QStringList arguments = app.arguments();
  const bool replaceExistingInstance =
      arguments.contains(QString::fromLatin1(kReplaceExistingInstanceArg));
  SingleInstanceGuard singleInstance(QString::fromLatin1(kInstanceKey),
                                     replaceExistingInstance);
  if (singleInstance.isSecondary()) {
    singleInstance.notifyPrimary();
    return 0;
  }

  AppBootstrapper bootstrapper(app);
  QObject::connect(&singleInstance,
                   &SingleInstanceGuard::activationRequested,
                   [&bootstrapper]() {
                     if (auto* window = bootstrapper.mainWindow()) {
                       window->showAndActivate();
                     }
                   });

  if (!bootstrapper.initialize()) {
    return -1;
  }
  const bool startHidden = app.arguments().contains("--hide");
  if (!bootstrapper.createUI()) {
    return -1;
  }
  bootstrapper.showMainWindow(startHidden);
  return app.exec();
}
