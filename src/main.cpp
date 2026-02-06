#include <QApplication>

#include "app/AppBootstrapper.h"
int main(int argc, char* argv[]) {
  // Enable high DPI support
  QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

  QApplication app(argc, argv);

  AppBootstrapper bootstrapper(app);
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
