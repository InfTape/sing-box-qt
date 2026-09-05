#include "Logger.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QLockFile>
#include <QTimer>
#include "utils/AppPaths.h"
#include "utils/LogRetention.h"

Logger& Logger::instance() {
  static Logger instance;
  return instance;
}

Logger::Logger() : m_initialized(false) {}

Logger::~Logger() {
  close();
}

void Logger::init() {
  {
    QMutexLocker locker(&m_mutex);
    if (m_initialized) {
      return;
    }
    m_logDirectory = QDir(appDataDir()).filePath("logs");
    if (!QDir().mkpath(m_logDirectory)) {
      return;
    }
    m_initialized = true;
    if (!m_maintenanceTimer && QCoreApplication::instance()) {
      m_maintenanceTimer = new QTimer(QCoreApplication::instance());
      m_maintenanceTimer->setObjectName("LogRetentionTimer");
      QObject::connect(m_maintenanceTimer, &QTimer::timeout,
                       m_maintenanceTimer, [this]() { maintainLogs(); });
    }
    if (m_maintenanceTimer) {
      m_maintenanceTimer->start(60 * 1000);
    }
  }
  maintainLogs();
  info("Logger initialized; keeping the last 24 hours.");
}

void Logger::close() {
  QMutexLocker locker(&m_mutex);
  if (m_maintenanceTimer) {
    m_maintenanceTimer->stop();
  }
  m_initialized = false;
}

void Logger::maintainLogs() {
  QMutexLocker locker(&m_mutex);
  if (m_initialized && !LogRetention::prune(m_logDirectory)) {
    qWarning("Could not finish log retention cleanup; will retry.");
  }
}

void Logger::log(const QString& level, const QString& message) {
  const auto now = QDateTime::currentDateTime();
  const QString logLine = QString("[%1] [%2] %3")
                              .arg(now.toString("yyyy-MM-dd hh:mm:ss.zzz"),
                                   level, message);
  qDebug().noquote() << logLine;
  QMutexLocker locker(&m_mutex);
  if (!m_initialized) {
    return;
  }
  QLockFile fileLock(LogRetention::lockFilePath(m_logDirectory));
  if (!fileLock.tryLock(1000)) {
    qWarning("Could not acquire log file lock.");
    return;
  }
  // Minute-sized segments can expire without rewriting a whole day's logs.
  QFile file(QDir(m_logDirectory).filePath(
      now.toString("yyyy-MM-dd-HH-mm") + ".log"));
  const QByteArray bytes = logLine.toUtf8() + '\n';
  if (!file.open(QIODevice::Append) || file.write(bytes) != bytes.size()) {
    qWarning("Could not write log file.");
  }
}

void Logger::debug(const QString& message) {
  instance().log("DEBUG", message);
}

void Logger::info(const QString& message) {
  instance().log("INFO", message);
}

void Logger::warn(const QString& message) {
  instance().log("WARN", message);
}

void Logger::error(const QString& message) {
  instance().log("ERROR", message);
}
