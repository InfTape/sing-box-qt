#include "KernelRunner.h"

#include <QFile>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTimer>

#include "core/ProcessManager.h"
#include "utils/AppPaths.h"
#include "utils/Logger.h"

KernelRunner::KernelRunner(QObject* parent)
    : QObject(parent), m_process(nullptr), m_running(false) {}

KernelRunner::~KernelRunner() {
  if (m_process) {
    if (m_process->state() != QProcess::NotRunning) {
      m_process->kill();
    }
    m_process->deleteLater();
  }
}

bool KernelRunner::start(const QString& configPath) {
  if (m_process && m_process->state() != QProcess::NotRunning) {
    Logger::warn(tr("Kernel is already running"));
    return false;
  }

  m_kernelPath = findKernelPath();
  if (m_kernelPath.isEmpty()) {
    m_lastError = tr("sing-box kernel not found");
    Logger::error(m_lastError);
    emit errorOccurred(m_lastError);
    return false;
  }

  if (!configPath.isEmpty()) {
    m_configPath = configPath;
  }
  if (m_configPath.isEmpty()) {
    m_configPath = getDefaultConfigPath();
  }

  if (!QFile::exists(m_configPath)) {
    m_lastError = tr("Config file not found");
    Logger::error(m_lastError);
    emit errorOccurred(m_lastError);
    return false;
  }

  if (m_process) {
    m_process->deleteLater();
  }

  m_process       = new QProcess(this);
  m_stopRequested = false;

  connect(m_process, &QProcess::started, this, &KernelRunner::onProcessStarted);
  connect(m_process, &QProcess::finished, this,
          &KernelRunner::onProcessFinished);
  connect(m_process, &QProcess::errorOccurred, this,
          &KernelRunner::onProcessError);
  connect(m_process, &QProcess::readyReadStandardOutput, this,
          &KernelRunner::onReadyReadStandardOutput);
  connect(m_process, &QProcess::readyReadStandardError, this,
          &KernelRunner::onReadyReadStandardError);

  QStringList args;
  args << "run" << "-c" << m_configPath;

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("ENABLE_DEPRECATED_SPECIAL_OUTBOUNDS", "true");
  m_process->setProcessEnvironment(env);

  Logger::info(tr("Starting kernel: %1").arg(m_kernelPath));
  m_process->start(m_kernelPath, args);

  if (!m_process->waitForStarted(5000)) {
    m_lastError = tr("Kernel failed to start");
    Logger::error(m_lastError);
    emit errorOccurred(m_lastError);
    return false;
  }

  return true;
}

void KernelRunner::stop() {
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

void KernelRunner::restart() { restartWithConfig(m_configPath); }

void KernelRunner::restartWithConfig(const QString& configPath) {
  setConfigPath(configPath);
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_restartPending = true;
    stop();
    return;
  }
  start(m_configPath);
}

void KernelRunner::setConfigPath(const QString& configPath) {
  m_configPath = configPath;
}

QString KernelRunner::getConfigPath() const { return m_configPath; }

bool KernelRunner::isRunning() const { return m_running; }

QString KernelRunner::getVersion() const {
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

QString KernelRunner::getKernelPath() const {
  return m_kernelPath.isEmpty() ? findKernelPath() : m_kernelPath;
}

QString KernelRunner::lastError() const { return m_lastError; }

void KernelRunner::onProcessStarted() {
  m_running = true;
  emit statusChanged(true);
}

void KernelRunner::onProcessFinished(int exitCode,
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

void KernelRunner::onProcessError(QProcess::ProcessError error) {
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

  m_lastError = errorMsg;
  Logger::error(errorMsg);
  emit errorOccurred(errorMsg);
}

void KernelRunner::onReadyReadStandardOutput() {
  QString output = QString::fromUtf8(m_process->readAllStandardOutput());
  Logger::info(QString("[Kernel] %1").arg(output.trimmed()));
  emit outputReceived(output);
}

void KernelRunner::onReadyReadStandardError() {
  QString error = QString::fromUtf8(m_process->readAllStandardError());
  Logger::error(QString("[Kernel Error] %1").arg(error.trimmed()));
  emit errorOutputReceived(error);
}

QString KernelRunner::findKernelPath() const {
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

QString KernelRunner::getDefaultConfigPath() const {
  const QString dataDir = appDataDir();
  return QDir(dataDir).filePath("config.json");
}
