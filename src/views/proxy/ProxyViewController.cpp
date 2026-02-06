#include "ProxyViewController.h"
#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThreadPool>
#include <QTimer>
#include <QtConcurrent>
#include "app/interfaces/ConfigRepository.h"
#include "core/DelayTestService.h"
#include "core/ProxyService.h"
#include "services/rules/RuleConfigService.h"
#include "storage/AppSettings.h"
#include "utils/proxy/ProxyActions.h"

ProxyViewController::ProxyViewController(ConfigRepository* configRepository,
                                         QObject*          parent)
    : QObject(parent), m_configRepository(configRepository) {}

void ProxyViewController::setProxyService(ProxyService* service) {
  if (m_proxyService == service) {
    return;
  }
  if (m_proxyService) {
    disconnect(m_proxyService, nullptr, this, nullptr);
  }
  m_proxyService = service;
  if (!m_proxyService) {
    return;
  }
  updateDelayTesterAuth();
  connect(m_proxyService,
          &ProxyService::proxiesReceived,
          this,
          &ProxyViewController::proxiesUpdated);
  connect(m_proxyService,
          &ProxyService::proxySelected,
          this,
          &ProxyViewController::proxySelected);
  connect(m_proxyService,
          &ProxyService::proxySelectFailed,
          this,
          &ProxyViewController::proxySelectFailed);
}

ProxyService* ProxyViewController::proxyService() const {
  return m_proxyService;
}

DelayTestService* ProxyViewController::ensureDelayTester() {
  if (!m_delayTestService) {
    m_delayTestService = std::make_unique<DelayTestService>(this);
    connect(m_delayTestService.get(),
            &DelayTestService::nodeDelayResult,
            this,
            &ProxyViewController::delayResult);
    connect(m_delayTestService.get(),
            &DelayTestService::testProgress,
            this,
            &ProxyViewController::testProgress);
    connect(m_delayTestService.get(),
            &DelayTestService::testCompleted,
            this,
            &ProxyViewController::testCompleted);
    connect(m_delayTestService.get(),
            &DelayTestService::errorOccurred,
            this,
            &ProxyViewController::errorOccurred);
  }
  updateDelayTesterAuth();
  return m_delayTestService.get();
}

bool ProxyViewController::isTesting() const {
  return m_delayTestService && m_delayTestService->isTesting();
}

void ProxyViewController::updateDelayTesterAuth() {
  if (!m_delayTestService || !m_proxyService) {
    return;
  }
  m_delayTestService->setApiPort(m_proxyService->getApiPort());
  m_delayTestService->setApiToken(m_proxyService->getApiToken());
}

void ProxyViewController::refreshProxies() const {
  if (m_proxyService) {
    m_proxyService->fetchProxies();
  }
}

void ProxyViewController::selectProxy(const QString& group,
                                      const QString& proxy) {
  if (!m_proxyService) {
    return;
  }
  ProxyActions::selectProxy(m_proxyService, group, proxy);
}

DelayTestOptions ProxyViewController::buildSingleOptions() const {
  DelayTestOptions options;
  options.timeoutMs   = AppSettings::instance().urltestTimeoutMs();
  options.url         = AppSettings::instance().urltestUrl();
  options.samples     = AppSettings::instance().urltestSamples();
  options.concurrency = 1;
  return options;
}

DelayTestOptions ProxyViewController::buildBatchOptions() const {
  DelayTestOptions options;
  options.timeoutMs   = AppSettings::instance().urltestTimeoutMs();
  options.url         = AppSettings::instance().urltestUrl();
  options.samples     = AppSettings::instance().urltestSamples();
  options.concurrency = AppSettings::instance().urltestConcurrency();
  return options;
}

void ProxyViewController::startSingleDelayTest(const QString& nodeName) {
  DelayTestService* tester = ensureDelayTester();
  if (!tester) {
    return;
  }
  tester->testNodeDelay(nodeName, buildSingleOptions());
}

void ProxyViewController::startBatchDelayTests(const QStringList& nodes) {
  DelayTestService* tester = ensureDelayTester();
  if (!tester) {
    return;
  }
  tester->testNodesDelay(nodes, buildBatchOptions());
}

void ProxyViewController::stopAllTests() {
  if (m_delayTestService) {
    m_delayTestService->stopAllTests();
  }
}

QJsonObject ProxyViewController::loadNodeOutbound(const QString& tag) const {
  const QString path = RuleConfigService::activeConfigPath(m_configRepository);
  if (path.isEmpty() || !m_configRepository) {
    return QJsonObject();
  }
  QJsonObject config = m_configRepository->loadConfig(path);
  if (config.isEmpty()) {
    return QJsonObject();
  }
  const QJsonArray outbounds = config.value("outbounds").toArray();
  for (const auto& v : outbounds) {
    if (!v.isObject()) {
      continue;
    }
    QJsonObject ob = v.toObject();
    if (ob.value("tag").toString() == tag) {
      return ob;
    }
  }
  return QJsonObject();
}

QString ProxyViewController::runBandwidthTest(const QString& nodeTag) const {
  Q_UNUSED(nodeTag)
  QNetworkAccessManager manager;
  QNetworkProxy         proxy(QNetworkProxy::Socks5Proxy,
                      "127.0.0.1",
                      AppSettings::instance().mixedPort());
  manager.setProxy(proxy);
  const QString testUrl =
      QStringLiteral("https://speed.cloudflare.com/__down?bytes=5000000");
  const int       timeoutMs = 15000;
  const QUrl      testQUrl(testUrl);
  QNetworkRequest request(testQUrl);
  QElapsedTimer   timer;
  timer.start();
  QNetworkReply* reply      = manager.get(request);
  qint64         totalBytes = 0;
  QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
    totalBytes += reply->bytesAvailable();
    reply->readAll();
  });
  QEventLoop loop;
  QTimer     timeout;
  timeout.setSingleShot(true);
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  timeout.start(timeoutMs + 2000);
  loop.exec();
  if (!reply->isFinished() || reply->error() != QNetworkReply::NoError) {
    reply->abort();
    reply->deleteLater();
    return QString();
  }
  totalBytes += reply->bytesAvailable();
  reply->readAll();
  reply->deleteLater();
  const qint64 ms = timer.elapsed();
  if (ms <= 0) {
    return QString();
  }
  const double bytesPerSec = (static_cast<double>(totalBytes) * 1000.0) / ms;
  const double mbps        = (bytesPerSec * 8.0) / (1024.0 * 1024.0);
  return tr("%1 Mbps").arg(QString::number(mbps, 'f', 1));
}

void ProxyViewController::startSpeedTest(const QString& nodeName,
                                         const QString& groupName) {
  if (!m_proxyService) {
    emit speedTestResult(nodeName, QString());
    return;
  }
  ProxyActions::selectProxy(m_proxyService, groupName, nodeName);
  QThreadPool::globalInstance()->start([this, nodeName]() {
    const QString result = runBandwidthTest(nodeName);
    QMetaObject::invokeMethod(
        this,
        [this, nodeName, result]() { emit speedTestResult(nodeName, result); },
        Qt::QueuedConnection);
  });
}
