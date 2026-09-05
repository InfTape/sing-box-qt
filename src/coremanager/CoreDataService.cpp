#include "coremanager/CoreDataService.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

#include "core/DataUsageTracker.h"
#include "storage/DatabaseService.h"
#include "storage/LogStore.h"
#include "utils/AppPaths.h"
#include "utils/LogParser.h"
#include "utils/Logger.h"

CoreDataService::CoreDataService(QObject* parent)
    : QObject(parent),
      m_tracker(new DataUsageTracker(this)),
      m_logStore(new LogStore(this)),
      m_nam(new QNetworkAccessManager(this)),
      m_connectionTimer(new QTimer(this)) {
  m_connectionTimer->setInterval(2000);
  connect(m_connectionTimer,
          &QTimer::timeout,
          this,
          &CoreDataService::pollConnections);
}

CoreDataService::~CoreDataService() {
  stop();
}

void CoreDataService::parseConfigApi(const QString& configPath) {
  m_configPath = configPath;
  if (configPath.isEmpty() || !QFile::exists(configPath)) {
    return;
  }
  QFile file(configPath);
  if (!file.open(QIODevice::ReadOnly)) {
    return;
  }
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isObject()) {
    return;
  }
  QJsonObject clashApi = doc.object()
                             .value("experimental")
                             .toObject()
                             .value("clash_api")
                             .toObject();
  QString ext = clashApi.value("external_controller").toString();
  int colon = ext.lastIndexOf(':');
  if (colon >= 0) {
    bool ok = false;
    int port = ext.mid(colon + 1).toInt(&ok);
    if (ok && port > 0) {
      m_apiPort = port;
    }
  }
  m_apiToken = clashApi.value("secret").toString();
}

void CoreDataService::start(const QString& configPath) {
  parseConfigApi(configPath);
  m_running = true;
  QTimer::singleShot(1000, this, [this]() {
    if (m_running) {
      m_connectionTimer->start();
      pollConnections();
    }
  });
}

void CoreDataService::stop() {
  m_running = false;
  if (m_connectionTimer) {
    m_connectionTimer->stop();
  }
  if (m_tracker) {
    m_tracker->flush();
  }
  if (m_logStore) {
    m_logStore->flush();
  }
}

void CoreDataService::pollConnections() {
  if (!m_running || m_apiPort <= 0) {
    return;
  }
  QUrl url(QString("http://127.0.0.1:%1/connections").arg(m_apiPort));
  QNetworkRequest request(url);
  if (!m_apiToken.isEmpty()) {
    request.setRawHeader("Authorization",
                         QString("Bearer %1").arg(m_apiToken).toUtf8());
  }
  QNetworkReply* reply = m_nam->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    if (reply->error() == QNetworkReply::NoError) {
      const QByteArray data = reply->readAll();
      const QJsonDocument doc = QJsonDocument::fromJson(data);
      if (doc.isObject() && m_tracker) {
        m_tracker->updateFromConnections(doc.object());
        emit dataUsageUpdated(m_tracker->snapshot());
      }
    }
  });
}

void CoreDataService::appendKernelOutput(const QString& stream,
                                         const QString& message) {
  Q_UNUSED(stream)
  if (message.isEmpty() || !m_logStore) {
    return;
  }
  const QStringList lines =
      message.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
  static const QRegularExpression ansiRegex(R"(\x1B\[[0-9;]*[a-zA-Z])");
  static const QRegularExpression levelPrefixRegex(
      R"(^(?:[+\-]\d{4}\s+)?(?:\d{4}[-/]\d{2}[-/]\d{2}\s+\d{2}:\d{2}:\d{2}(?:\.\d+)?\s+)?\[?(INFO|WARN|WARNING|ERROR|DEBUG|TRACE|FATAL|PANIC)\]?(?:\[[^\]]*\])?[:\s]*)",
      QRegularExpression::CaseInsensitiveOption);

  for (const QString& line : lines) {
    QString clean = line.trimmed();
    clean.remove(ansiRegex);
    if (clean.isEmpty()) {
      continue;
    }
    QString type = "info";
    QString stripped = clean;
    const QRegularExpressionMatch levelMatch = levelPrefixRegex.match(clean);
    if (levelMatch.hasMatch()) {
      const QString levelStr = levelMatch.captured(1).toLower();
      if (levelStr == "error" || levelStr == "fatal" || levelStr == "panic") {
        type = "error";
      } else if (levelStr == "warn" || levelStr == "warning") {
        type = "warning";
      } else if (levelStr == "debug") {
        type = "debug";
      } else if (levelStr == "trace") {
        type = "trace";
      } else {
        type = "info";
      }
      stripped = clean.mid(levelMatch.capturedLength()).trimmed();
    } else {
      if (clean.contains("error", Qt::CaseInsensitive)) {
        type = "error";
      } else if (clean.contains("warn", Qt::CaseInsensitive)) {
        type = "warning";
      } else {
        type = "info";
      }
    }
    if (stripped.isEmpty()) {
      stripped = clean;
    }

    const LogParser::LogKind kind = LogParser::parseLogKind(clean);
    LogParser::LogEntry      entry;
    entry.type      = type;
    entry.timestamp = QDateTime::currentDateTime();

    if (kind.isConnection && entry.type == "info") {
      QString label;
      if (!kind.protocol.isEmpty() && !kind.nodeName.isEmpty()) {
        label = QString("%1[%2]").arg(kind.protocol, kind.nodeName);
      } else if (!kind.protocol.isEmpty()) {
        label = kind.protocol;
      } else if (!kind.nodeName.isEmpty()) {
        label = QString("[%1]").arg(kind.nodeName);
      }
      if (kind.direction == "outbound") {
        if (!label.isEmpty() && !kind.host.isEmpty()) {
          entry.payload = QString("%1 -> %2").arg(label, kind.host);
        } else if (!kind.host.isEmpty()) {
          entry.payload = QString("outbound -> %1").arg(kind.host);
        } else {
          entry.payload = label.isEmpty() ? stripped : label;
        }
      } else if (kind.direction == "inbound") {
        if (!label.isEmpty() && !kind.host.isEmpty()) {
          entry.payload = QString("%1 <- %2").arg(label, kind.host);
        } else if (!kind.host.isEmpty()) {
          entry.payload = QString("inbound <- %1").arg(kind.host);
        } else {
          entry.payload = label.isEmpty() ? stripped : label;
        }
      } else {
        entry.payload = stripped;
      }
      entry.direction = kind.direction;
      entry.network = kind.network;
    } else if (kind.isDns) {
      entry.payload   = stripped;
      entry.direction = kind.direction;
    } else {
      entry.payload = stripped;
    }

    m_logStore->append(entry);
    emit apiLogMessage(entry.type, entry.payload);
  }
  m_logStore->flush();
}

QJsonObject CoreDataService::dataUsageSnapshot(int limit) const {
  return m_tracker ? m_tracker->snapshot(limit) : QJsonObject();
}

void CoreDataService::resetDataUsage() {
  if (m_tracker) {
    m_tracker->reset();
    emit dataUsageUpdated(m_tracker->snapshot());
  }
}
