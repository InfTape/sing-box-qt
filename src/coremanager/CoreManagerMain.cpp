#include <QCoreApplication>
#include "core/CoreManagerProtocol.h"
#include "coremanager/CoreManagerServer.h"
#include "utils/Logger.h"

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  app.setApplicationName("sing-box-core-manager");
  Logger::instance().init();
  Logger::info("Core manager starting...");
  QString           serverName = coreManagerServerName();
  const QStringList args       = app.arguments();
  for (int i = 1; i < args.size(); ++i) {
    if (args[i] == "--control-name" && i + 1 < args.size()) {
      serverName = args[i + 1];
      ++i;
    }
  }
  CoreManagerServer server;
  QString           error;
  if (!server.startListening(serverName, &error)) {
    Logger::error(error);
    return 1;
  }
  return app.exec();
}
