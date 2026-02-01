#include "KernelService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

#include "core/ProcessManager.h"
#include "utils/AppPaths.h"
#include "utils/Logger.h"
KernelService::KernelService(QObject* parent)
    : QObject(parent), m_process(nullptr), m_running(false) {}
KernelService::~KernelService() {
  if (m_process) {
    if (m_process->state() != QProcess::NotRunning) {
      m_process->kill();
    }
    m_process->deleteLater();
  }
}
bool KernelService::start(const QString& configPath) {
  if (m_process && m_process->state() != QProcess::NotRunning) {
    Logger::warn(tr("Kernel is already running"));
    return false;
  }

  m_kernelPath = findKernelPath();
  if (m_kernelPath.isEmpty()) {
    Logger::error(tr("sing-box kernel not found"));
    emit errorOccurred(tr("sing-box kernel not found"));
    return false;
  }

  if (!configPath.isEmpty()) {
    m_configPath = configPath;
  }
  if (m_configPath.isEmpty()) {
    m_configPath = getDefaultConfigPath();
  }

  if (!QFile::exists(m_configPath)) {
    Logger::error(tr("Config file not found"));
    emit errorOccurred(tr("Config file not found"));
    return false;
  }

  if (m_process) {
    m_process->deleteLater();
  }

  m_process       = new QProcess(this);
  m_stopRequested = false;

  connect(m_process, &QProcess::started, this,
          &KernelService::onProcessStarted);
  connect(m_process, &QProcess::finished, this,
          &KernelService::onProcessFinished);
  connect(m_process, &QProcess::errorOccurred, this,
          &KernelService::onProcessError);
  connect(m_process, &QProcess::readyReadStandardOutput, this,
          &KernelService::onReadyReadStandardOutput);
  connect(m_process, &QProcess::readyReadStandardError, this,
          &KernelService::onReadyReadStandardError);

  QStringList args;
  args << "run" << "-c" << m_configPath;

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("ENABLE_DEPRECATED_SPECIAL_OUTBOUNDS", "true");
  m_process->setProcessEnvironment(env);

  Logger::info(tr("Starting kernel: %1").arg(m_kernelPath));
  m_process->start(m_kernelPath, args);

  if (!m_process->waitForStarted(5000)) {
    Logger::error(tr("Kernel failed to start"));
    emit errorOccurred(tr("Kernel failed to start"));
    return false;
  }

  return true;
}
void KernelService::stop() {
  if (!m_process || m_process->state() == QProcess::NotRunning) {
    Logger::warn(tr("Kernel is not running"));
    return;
  }

  m_stopRequested = true;
  Logger::info(tr("Stopping kernel..."));

  m_process->terminate();

  QTimer::singleShot(3000, this, [this]() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
#ifdef Q_OS_WIN
      const qint64 pid = m_process->processId();
      if (pid > 0) {
        ProcessManager::killProcess(pid);
      } else {
        m_process->kill();
      }
#else
            m_process->kill();
#endif
    }
  });
}
void KernelService::restart() { restartWithConfig(m_configPath); }
void KernelService::restartWithConfig(const QString& configPath) {
  setConfigPath(configPath);
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_restartPending = true;
    stop();
    return;
  }
  start(m_configPath);
}
void KernelService::setConfigPath(const QString& configPath) {
  m_configPath = configPath;
}
QString KernelService::getConfigPath() const { return m_configPath; }
bool    KernelService::isRunning() const { return m_running; }
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
void KernelService::onProcessStarted() {
  m_running = true;
  emit statusChanged(true);
}
void KernelService::onProcessFinished(int                  exitCode,
                                      QProcess::ExitStatus exitStatus) {
  Q_UNUSED(exitCode)
  Q_UNUSED(exitStatus)
  m_running = false;
  emit statusChanged(false);

  if (m_restartPending) {
    m_restartPending = false;
    start(m_configPath);
  }
}
void KernelService::onProcessError(QProcess::ProcessError error) {
  QString errorMsg;
  switch (error) {
    case QProcess::FailedToStart:
      errorMsg = tr("Kernel failed to start");
      break;
    case QProcess::Crashed:
      if (m_stopRequested) {
        Logger::info(tr("Kernel stopped"));
        return;
      }
      errorMsg = tr("Kernel crashed");
      break;
    case QProcess::Timedout:
      errorMsg = tr("Kernel response timed out");
      break;
    default:
      errorMsg = tr("Unknown kernel error");
      break;
  }

  Logger::error(errorMsg);
  emit errorOccurred(errorMsg);
}
void KernelService::onReadyReadStandardOutput() {
  QString output = QString::fromUtf8(m_process->readAllStandardOutput());
  Logger::info(QString("[Kernel] %1").arg(output.trimmed()));
  emit outputReceived(output);
}
void KernelService::onReadyReadStandardError() {
  QString error = QString::fromUtf8(m_process->readAllStandardError());
  Logger::error(QString("[Kernel Error] %1").arg(error.trimmed()));
  emit outputReceived(error);
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
