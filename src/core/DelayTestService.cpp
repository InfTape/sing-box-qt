#include "DelayTestService.h"
#include "network/HttpClient.h"
#include "storage/AppSettings.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QThread>
#include <QtConcurrent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QAtomicInt>
#include <algorithm>

DelayTestService::DelayTestService(QObject *parent)
    : QObject(parent)
    , m_httpClient(new HttpClient(this))
    , m_apiPort(9090)
    , m_stopping(false)
    , m_semaphore(nullptr)
    , m_activeTasks(0)
{
}

DelayTestService::~DelayTestService()
{
    stopAllTests();
    delete m_semaphore;
}

void DelayTestService::setApiPort(int port)
{
    m_apiPort = port;
}

void DelayTestService::setApiToken(const QString &token)
{
    m_apiToken = token.trimmed();
}

bool DelayTestService::isTesting() const
{
    return m_activeTasks.load() > 0;
}

void DelayTestService::stopAllTests()
{
    QMutexLocker locker(&m_mutex);
    m_stopping = true;
    if (m_semaphore) {
        m_semaphore->release(m_semaphore->available() + 1);
    }
}

QString DelayTestService::buildClashDelayUrl(const QString &proxy, int timeoutMs, const QString &testUrl) const
{
    // Build Clash API delay URL.
    // /proxies/{name}/delay?timeout=xxx&url=xxx

    // Percent-encode proxy name (keep UTF-8).
    QString encodedProxy = QString::fromUtf8(QUrl::toPercentEncoding(proxy));

    QUrl url;
    url.setScheme("http");
    url.setHost("127.0.0.1");
    url.setPort(m_apiPort);
    url.setPath(QString("/proxies/%1/delay").arg(encodedProxy));

    QUrlQuery query;
    query.addQueryItem("timeout", QString::number(timeoutMs));
    query.addQueryItem("url", testUrl);
    url.setQuery(query);

    return url.toString();
}

namespace {
QString resolveTestUrl(const DelayTestOptions &options)
{
    const QString candidate = options.url.trimmed();
    if (!candidate.isEmpty()) {
        return candidate;
    }
    return AppSettings::instance().urltestUrl();
}
} // namespace

int DelayTestService::fetchSingleDelay(const QString &proxy, int timeoutMs, const QString &testUrl)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_stopping) {
            return -1;
        }
    }

    QString url = buildClashDelayUrl(proxy, timeoutMs, testUrl);

    // Create a local QNetworkAccessManager to avoid cross-thread issues.
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_apiToken.isEmpty()) {
        request.setRawHeader("Authorization", QByteArray("Bearer ").append(m_apiToken.toUtf8()));
    }
    request.setTransferTimeout(timeoutMs + 2000);

    QNetworkReply *reply = manager.get(request);

    // Wait for request to finish.
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    // Set timeout.
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs + 3000);

    loop.exec();

    int result = -1;

    if (reply->isFinished() && reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject() && doc.object().contains("delay")) {
            int delay = doc.object()["delay"].toInt();
            if (delay > 0) {
                result = delay;
            }
        }
    }

    // Delete reply directly since manager is local.
    delete reply;
    return result;
}

int DelayTestService::medianValue(QVector<int> &values)
{
    if (values.isEmpty()) {
        return -1;
    }
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

ProxyDelayTestResult DelayTestService::measureProxyDelay(const QString &proxy, const DelayTestOptions &options)
{
    ProxyDelayTestResult result;
    result.proxy = proxy;

    const QString testUrl = resolveTestUrl(options);

    QVector<int> okValues;
    QString lastError;

    int samples = qMax(1, options.samples);

    for (int i = 0; i < samples; ++i) {
        // Check stop flag.
        {
            QMutexLocker locker(&m_mutex);
            if (m_stopping) {
                result.error = tr("Test canceled");
                return result;
            }
        }

        int delay = fetchSingleDelay(proxy, options.timeoutMs, testUrl);

        if (delay > 0) {
            okValues.append(delay);
        } else {
            lastError = tr("Request failed");
        }

        // Sample interval to avoid spikes on the same proxy.
        if (i < samples - 1) {
            QThread::msleep(80);
        }
    }

    if (!okValues.isEmpty()) {
        result.delay = medianValue(okValues);
        result.ok = true;
        result.successSamples = okValues.size();
    } else {
        result.delay = 0;
        result.ok = false;
        result.error = lastError.isEmpty() ? tr("No valid result") : lastError;
        result.successSamples = 0;
    }

    return result;
}

void DelayTestService::testNodeDelay(const QString &proxy, const DelayTestOptions &options)
{
    m_activeTasks.store(1);

    m_lastFuture = QtConcurrent::run([this, proxy, options]() {
        ProxyDelayTestResult result = measureProxyDelay(proxy, options);

        // Emit on the main thread.
        QMetaObject::invokeMethod(this, [this, result]() {
            emit nodeDelayResult(result);
            m_activeTasks.store(0);
        }, Qt::QueuedConnection);
    });
}

void DelayTestService::testNodesDelay(const QStringList &proxies, const DelayTestOptions &options)
{
    if (proxies.isEmpty()) {
        emit testCompleted();
        return;
    }

    // Reset stop flag.
    {
        QMutexLocker locker(&m_mutex);
        m_stopping = false;
    }

    // 并发数量与 Throne-dev 一致：默认 10，最大 100，最小 1。
    const int maxConcurrency = std::clamp(options.concurrency, 1, 100);

    // Create semaphore for concurrency control.
    delete m_semaphore;
    m_semaphore = new QSemaphore(maxConcurrency);

    int total = proxies.size();
    m_activeTasks.store(total);

    m_lastFuture = QtConcurrent::run([this, proxies, options, total]() {
        QAtomicInt completed(0);
        QList<QFuture<void>> futures;

        for (const QString &proxy : proxies) {
            // Check stop flag.
            {
                QMutexLocker locker(&m_mutex);
                if (m_stopping) {
                    break;
                }
            }

            // Acquire semaphore (limit concurrency).
            m_semaphore->acquire();

            auto future = QtConcurrent::run([this, proxy, options, total, &completed]() {
                ProxyDelayTestResult result = measureProxyDelay(proxy, options);

                // Release semaphore.
                m_semaphore->release();

                int current = completed.fetchAndAddRelaxed(1) + 1;

                // Emit on the main thread.
                QMetaObject::invokeMethod(this, [this, result, current, total]() {
                    emit nodeDelayResult(result);
                    emit testProgress(current, total);
                    m_activeTasks.fetch_sub(1);
                }, Qt::QueuedConnection);
            });

            futures.append(future);
        }

        // Wait for all tasks to finish.
        for (auto &future : futures) {
            future.waitForFinished();
        }

        // Emit completion.
        QMetaObject::invokeMethod(this, [this]() {
            emit testCompleted();
            m_activeTasks.store(0);
        }, Qt::QueuedConnection);
    });
}

void DelayTestService::testGroupDelay(const QString &group, const QJsonArray &nodes, const DelayTestOptions &options)
{
    Q_UNUSED(group)

    QStringList proxies;
    for (const auto &node : nodes) {
        proxies.append(node.toString());
    }

    testNodesDelay(proxies, options);
}
