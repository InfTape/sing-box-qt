#include "KernelService.h"
#include "utils/Logger.h"
#include "core/ProcessManager.h"
#include <QStandardPaths>
#include <QCoreApplication>
#include <QFile>
#include <QTimer>
#include <QDir>
#include <QRegularExpression>
#include "utils/AppPaths.h"

KernelService::KernelService(QObject *parent)
    : QObject(parent)
    , m_process(nullptr)
    , m_running(false)
{
}

KernelService::~KernelService()
{
    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
        }
        m_process->deleteLater();
    }
}

bool KernelService::start(const QString &configPath)
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        Logger::warn(tr("内核已在运行"));
        return false;
    }

    m_kernelPath = findKernelPath();
    if (m_kernelPath.isEmpty()) {
        Logger::error(tr("未找到 sing-box 内核"));
        emit errorOccurred(tr("未找到 sing-box 内核"));
        return false;
    }

    if (!configPath.isEmpty()) {
        m_configPath = configPath;
    }
    if (m_configPath.isEmpty()) {
        m_configPath = getDefaultConfigPath();
    }

    if (!QFile::exists(m_configPath)) {
        Logger::error(tr("未找到配置文件"));
        emit errorOccurred(tr("未找到配置文件"));
        return false;
    }

    if (m_process) {
        m_process->deleteLater();
    }

    m_process = new QProcess(this);
    m_stopRequested = false;

    connect(m_process, &QProcess::started, this, &KernelService::onProcessStarted);
    connect(m_process, &QProcess::finished, this, &KernelService::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &KernelService::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &KernelService::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &KernelService::onReadyReadStandardError);

    QStringList args;
    args << "run" << "-c" << m_configPath;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("ENABLE_DEPRECATED_SPECIAL_OUTBOUNDS", "true");
    m_process->setProcessEnvironment(env);

    Logger::info(tr("启动内核: %1").arg(m_kernelPath));
    m_process->start(m_kernelPath, args);

    if (!m_process->waitForStarted(5000)) {
        Logger::error(tr("内核启动失败"));
        emit errorOccurred(tr("内核启动失败"));
        return false;
    }

    return true;
}

void KernelService::stop()
{
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        Logger::warn(tr("内核未运行"));
        return;
    }

    m_stopRequested = true;
    Logger::info(tr("停止内核..."));

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

void KernelService::restart()
{
    stop();
    QTimer::singleShot(1000, this, [this]() {
        start();
    });
}

void KernelService::restartWithConfig(const QString &configPath)
{
    setConfigPath(configPath);
    restart();
}

void KernelService::setConfigPath(const QString &configPath)
{
    m_configPath = configPath;
}

QString KernelService::getConfigPath() const
{
    return m_configPath;
}

bool KernelService::isRunning() const
{
    return m_running;
}

QString KernelService::getVersion() const
{
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

    const QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    QRegularExpression re("(\\d+\\.\\d+\\.\\d+)");
    QRegularExpressionMatch match = re.match(output);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    return output;
}

QString KernelService::getKernelPath() const
{
    return m_kernelPath.isEmpty() ? findKernelPath() : m_kernelPath;
}

void KernelService::onProcessStarted()
{
    m_running = true;
    emit statusChanged(true);
}

void KernelService::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)
    m_running = false;
    emit statusChanged(false);
}

void KernelService::onProcessError(QProcess::ProcessError error)
{
    QString errorMsg;
    switch (error) {
    case QProcess::FailedToStart:
        errorMsg = tr("内核启动失败");
        break;
    case QProcess::Crashed:
        if (m_stopRequested) {
            Logger::info(tr("内核已停止"));
            return;
        }
        errorMsg = tr("内核崩溃");
        break;
    case QProcess::Timedout:
        errorMsg = tr("内核响应超时");
        break;
    default:
        errorMsg = tr("内核发生未知错误");
        break;
    }

    Logger::error(errorMsg);
    emit errorOccurred(errorMsg);
}

void KernelService::onReadyReadStandardOutput()
{
    QString output = QString::fromUtf8(m_process->readAllStandardOutput());
    Logger::info(QString("[Kernel] %1").arg(output.trimmed()));
    emit outputReceived(output);
}

void KernelService::onReadyReadStandardError()
{
    QString error = QString::fromUtf8(m_process->readAllStandardError());
    Logger::error(QString("[Kernel Error] %1").arg(error.trimmed()));
    emit outputReceived(error);
}

QString KernelService::findKernelPath() const
{
#ifdef Q_OS_WIN
    QString kernelName = "sing-box.exe";
#else
    QString kernelName = "sing-box";
#endif

    const QString dataDir = appDataDir();
    Logger::info(QString("正在查找内核，数据目录: %1").arg(dataDir));

    const QString path = QDir(dataDir).filePath(kernelName);
    Logger::info(QString("尝试路径: %1").arg(path));
    if (QFile::exists(path)) {
        Logger::info(QString("找到内核: %1").arg(path));
        return path;
    }

    Logger::warn("未找到 sing-box 内核");
    return QString();
}

QString KernelService::getDefaultConfigPath() const
{
    const QString dataDir = appDataDir();
    return QDir(dataDir).filePath("config.json");
}
