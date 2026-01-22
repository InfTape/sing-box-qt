#include "Logger.h"
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QDebug>
#include "utils/AppPaths.h"

Logger& Logger::instance()
{
    static Logger instance;
    return instance;
}

Logger::Logger()
    : m_initialized(false)
{
}

Logger::~Logger()
{
    close();
}

void Logger::init()
{
    if (m_initialized) {
        return;
    }
    
    QString logPath = getLogFilePath();
    
    // 确保日志目录存在
    QDir dir(QFileInfo(logPath).absolutePath());
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    m_logFile.setFileName(logPath);
    if (m_logFile.open(QIODevice::Append | QIODevice::Text)) {
        m_initialized = true;
        info("日志系统初始化完成");
    }
}

void Logger::close()
{
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
    m_initialized = false;
}

QString Logger::getLogFilePath() const
{
    QString dataDir = appDataDir();
    QString date = QDate::currentDate().toString("yyyy-MM-dd");
    return dataDir + "/logs/" + date + ".log";
}

void Logger::log(const QString &level, const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString logLine = QString("[%1] [%2] %3").arg(timestamp, level, message);
    
    // 输出到控制台
    qDebug().noquote() << logLine;
    
    // 输出到文件
    if (m_initialized) {
        QMutexLocker locker(&m_mutex);
        QTextStream stream(&m_logFile);
        stream << logLine << "\n";
        stream.flush();
    }
}

void Logger::debug(const QString &message)
{
    instance().log("DEBUG", message);
}

void Logger::info(const QString &message)
{
    instance().log("INFO", message);
}

void Logger::warn(const QString &message)
{
    instance().log("WARN", message);
}

void Logger::error(const QString &message)
{
    instance().log("ERROR", message);
}
