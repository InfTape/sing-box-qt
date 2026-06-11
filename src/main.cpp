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
