#include "KernelService.h"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>
#include "core/CoreManagerClient.h"
#include "core/CoreManagerProtocol.h"
#include "utils/AppPaths.h"
#include "utils/Logger.h"

KernelService::KernelService(QObject* parent)
    : QObject(parent),
      m_client(new CoreManagerClient(this)),
      m_managerProcess(nullptr),
      m_running(false) {
  connect(m_client,
          &CoreManagerClient::statusEvent,
          this,
          &KernelService::onManagerStatus);
  connect(m_client,
          &CoreManagerClient::logEvent,
          this,
          &KernelService::onManagerLog);
  connect(m_client,
          &CoreManagerClient::errorEvent,
          this,
          &KernelService::onManagerError);
  connect(m_client,
          &CoreManagerClient::disconnected,
          this,
          &KernelService::onManagerDisconnected);
}

KernelService::~KernelService() {
  const bool managerRunning =
      (m_managerProcess && m_managerProcess->state() != QProcess::NotRunning) ||
      m_client->isConnected();
  if (m_spawnedManager && managerRunning) {
    QJsonObject result;
    QString     error;
    sendRequestAndWait("shutdown", QJsonObject(), &result, &error);
  }
  if (m_managerProcess) {
    if (m_managerProcess->state() != QProcess::NotRunning) {
      m_managerProcess->waitForFinished(2000);
      if (m_managerProcess->state() != QProcess::NotRunning) {
        m_managerProcess->kill();
      }
    }
    m_managerProcess->deleteLater();
  }
}

bool KernelService::start(const QString& configPath) {
  if (!configPath.isEmpty()) {
    m_configPath = configPath;
  }
  if (m_configPath.isEmpty()) {
    m_configPath = getDefaultConfigPath();
  }
  if (!QFile::exists(m_configPath)) {
    const QString msg = tr("Config file not found");
    Logger::error(msg);
    emit errorOccurred(msg);
    return false;
  }
  QString     error;
  QJsonObject result;
  if (!sendRequestAndWait("start",
                          QJsonObject{{"configPath", m_configPath}},
                          &result,
                          &error)) {
    if (!error.isEmpty()) {
      emit errorOccurred(error);
    }
    return false;
  }
  return true;
}

void KernelService::stop() {
  QJsonObject result;
  QString     error;
  if (!sendRequestAndWait("stop", QJsonObject(), &result, &error)) {
    if (!error.isEmpty()) {
      emit errorOccurred(error);
    }
  }
}

void KernelService::restart() {
  restartWithConfig(m_configPath);
}

void KernelService::restartWithConfig(const QString& configPath) {
  if (!configPath.isEmpty()) {
    m_configPath = configPath;
  }
  QJsonObject result;
  QString     error;
  QJsonObject params;
  if (!m_configPath.isEmpty()) {
    params["configPath"] = m_configPath;
  }
  if (!sendRequestAndWait("restart", params, &result, &error)) {
    if (!error.isEmpty()) {
      emit errorOccurred(error);
    }
  }
}

void KernelService::setConfigPath(const QString& configPath) {
  m_configPath = configPath;
}

QString KernelService::getConfigPath() const {
  return m_configPath;
}

bool KernelService::isRunning() const {
  return m_running;
}

QString KernelService::getVersion() const {
  QString kernelPath = m_kernelPath;
  if (kernelPath.isEmpty()) {
    kernelPath = findKernelPath();
  }
  if (kernelPath.isEmpty() || !QFile::exists(kernelPath)) {
    return QString();
  }
  QProcess process;
  process.start(kernelPath, {"version"});
  if (!process.waitForFinished(3000)) {
    process.kill();
    return QString();
  }
  const QString output =
      QString::fromUtf8(process.readAllStandardOutput()).trimmed();
  QRegularExpression      re("(\\d+\\.\\d+\\.\\d+)");
  QRegularExpressionMatch match = re.match(output);
  if (match.hasMatch()) {
    return match.captured(1);
  }
  return output;
}

QString KernelService::getKernelPath() const {
  return m_kernelPath.isEmpty() ? findKernelPath() : m_kernelPath;
}

void KernelService::onManagerStatus(bool running) {
  if (m_running == running) {
    return;
  }
  m_running = running;
  emit statusChanged(running);
}

void KernelService::onManagerLog(const QString& stream,
                                 const QString& message) {
  Q_UNUSED(stream)
  emit outputReceived(message);
}

void KernelService::onManagerError(const QString& error) {
  if (!error.isEmpty()) {
    emit errorOccurred(error);
  }
}

void KernelService::onManagerDisconnected() {
  if (m_running) {
    m_running = false;
    emit statusChanged(false);
  }
}

bool KernelService::ensureManagerReady(QString* error) {
  if (m_serverName.isEmpty()) {
    m_serverName = coreManagerServerName();
  }
  if (m_client->isConnected()) {
    return true;
  }
  m_client->connectToServer(m_serverName);
  if (m_client->waitForConnected(300)) {
    return true;
  }
  m_client->abort();
  if (!m_managerProcess || m_managerProcess->state() == QProcess::NotRunning) {
    const QString exePath = findCoreManagerPath();
    if (exePath.isEmpty() || !QFile::exists(exePath)) {
      if (error) {
        *error = tr("Core manager not found");
      }
      return false;
    }
    if (!m_managerProcess) {
      m_managerProcess = new QProcess(this);
    }
    const QString workDir = appDataDir();
    if (!QDir().mkpath(workDir)) {
      Logger::error(
          QString("Failed to prepare working directory: %1").arg(workDir));
      if (error) {
        *error = tr("Failed to prepare working directory");
      }
      return false;
    }
    m_managerProcess->setWorkingDirectory(workDir);
    QStringList args;
    args << "--control-name" << m_serverName;
    m_managerProcess->start(exePath, args);
    if (!m_managerProcess->waitForStarted(3000)) {
      if (error) {
        *error = tr("Failed to start core manager");
      }
      return false;
    }
    m_spawnedManager = true;
  }
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < 3000) {
    m_client->connectToServer(m_serverName);
    if (m_client->waitForConnected(300)) {
      return true;
    }
    m_client->abort();
  }
  if (error) {
    *error = tr("Failed to connect to core manager");
  }
  return false;
}

bool KernelService::sendRequestAndWait(const QString&     method,
                                       const QJsonObject& params,
                                       QJsonObject*       result,
                                       QString*           error) {
  QString err;
  if (!ensureManagerReady(&err)) {
    if (error) {
      *error = err;
    }
    return false;
  }
  const int   id = m_nextRequestId++;
  bool        ok = false;
  QString     respError;
  QJsonObject respResult;
  QEventLoop  loop;
  QTimer      timer;
  timer.setSingleShot(true);
  QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
  QMetaObject::Connection conn = connect(m_client,
                                         &CoreManagerClient::responseReceived,
                                         this,
                                         [&](int                respId,
                                             bool               respOk,
                                             const QJsonObject& resp,
                                             const QString&     respErr) {
                                           if (respId != id) {
                                             return;
                                           }
                                           ok         = respOk;
                                           respResult = resp;
                                           respError  = respErr;
                                           loop.quit();
                                         });
  m_client->sendRequest(id, method, params);
  timer.start(5000);
  loop.exec();
  disconnect(conn);
  if (timer.isActive() == false) {
    if (error) {
      *error = tr("RPC timeout");
    }
    return false;
  }
  if (!ok && error) {
    *error = respError;
  }
  if (result) {
    *result = respResult;
  }
  return ok;
}

QString KernelService::findKernelPath() const {
#ifdef Q_OS_WIN
  QString kernelName = "sing-box.exe";
#else
  QString kernelName = "sing-box";
#endif
  const QString dataDir = appDataDir();
  Logger::info(QString("Searching for kernel, data dir: %1").arg(dataDir));
  const QString path = QDir(dataDir).filePath(kernelName);
  Logger::info(QString("Trying path: %1").arg(path));
  if (QFile::exists(path)) {
    Logger::info(QString("Kernel found: %1").arg(path));
    return path;
  }
  Logger::warn("sing-box kernel not found");
  return QString();
}

QString KernelService::getDefaultConfigPath() const {
  const QString dataDir = appDataDir();
  return QDir(dataDir).filePath("config.json");
}

QString KernelService::findCoreManagerPath() const {
  const QString exeName = coreManagerExecutableName();
  return QDir(QCoreApplication::applicationDirPath()).filePath(exeName);
}
