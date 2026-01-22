#include "DelayTestService.h"
#include "network/HttpClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QThread>
#include <QtConcurrent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <algorithm>

DelayTestService::DelayTestService(QObject *parent)
    : QObject(parent)
    , m_httpClient(new HttpClient(this))
    , m_apiPort(9090)
    , m_stopping(false)
    , m_semaphore(nullptr)
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

bool DelayTestService::isTesting() const
{
    QMutexLocker locker(&m_mutex);
    return m_semaphore != nullptr && m_semaphore->available() < 6;
}

void DelayTestService::stopAllTests()
{
    QMutexLocker locker(&m_mutex);
    m_stopping = true;
}

QString DelayTestService::buildClashDelayUrl(const QString &proxy, int timeoutMs, const QString &testUrl) const
{
    // 构建 Clash API 延迟测试 URL
    // /proxies/{name}/delay?timeout=xxx&url=xxx
    
    // 正确编码节点名称（保持 UTF-8）
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

int DelayTestService::fetchSingleDelay(const QString &proxy, int timeoutMs, const QString &testUrl)
{
    QString url = buildClashDelayUrl(proxy, timeoutMs, testUrl);
    
    // 在当前线程创建独立的 QNetworkAccessManager，避免跨线程问题
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(timeoutMs + 2000);
    
    QNetworkReply *reply = manager.get(request);
    
    // 使用事件循环等待请求完成
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    // 设置超时
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
    
    // 直接删除 reply，因为 manager 是局部变量，函数返回后会被销毁
    // 不能使用 deleteLater()，否则会在 manager 销毁后尝试删除
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
    
    QVector<int> okValues;
    QString lastError;
    
    int samples = qMax(1, options.samples);
    
    for (int i = 0; i < samples; ++i) {
        // 检查是否停止
        {
            QMutexLocker locker(&m_mutex);
            if (m_stopping) {
                result.error = tr("测试已取消");
                return result;
            }
        }
        
        int delay = fetchSingleDelay(proxy, options.timeoutMs, options.url);
        
        if (delay > 0) {
            okValues.append(delay);
        } else {
            lastError = tr("请求失败");
        }
        
        // 采样间隔，避免对同一节点瞬时并发过高
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
        result.error = lastError.isEmpty() ? tr("无可用结果") : lastError;
        result.successSamples = 0;
    }
    
    return result;
}

void DelayTestService::testNodeDelay(const QString &proxy, const DelayTestOptions &options)
{
    // 在后台线程执行测试
    QtConcurrent::run([this, proxy, options]() {
        ProxyDelayTestResult result = measureProxyDelay(proxy, options);
        
        // 在主线程发射信号
        QMetaObject::invokeMethod(this, [this, result]() {
            emit nodeDelayResult(result);
        }, Qt::QueuedConnection);
    });
}

void DelayTestService::testNodesDelay(const QStringList &proxies, const DelayTestOptions &options)
{
    if (proxies.isEmpty()) {
        emit testCompleted();
        return;
    }
    
    // 重置停止标志
    {
        QMutexLocker locker(&m_mutex);
        m_stopping = false;
    }
    
    // 创建信号量控制并发
    delete m_semaphore;
    m_semaphore = new QSemaphore(qMax(1, options.concurrency));
    
    int total = proxies.size();
    
    QtConcurrent::run([this, proxies, options, total]() {
        QAtomicInt completed(0);
        QList<QFuture<void>> futures;
        
        for (const QString &proxy : proxies) {
            // 检查是否停止
            {
                QMutexLocker locker(&m_mutex);
                if (m_stopping) {
                    break;
                }
            }
            
            // 获取信号量（控制并发）
            m_semaphore->acquire();
            
            auto future = QtConcurrent::run([this, proxy, options, total, &completed]() {
                ProxyDelayTestResult result = measureProxyDelay(proxy, options);
                
                // 释放信号量
                m_semaphore->release();
                
                int current = completed.fetchAndAddRelaxed(1) + 1;
                
                // 在主线程发射信号
                QMetaObject::invokeMethod(this, [this, result, current, total]() {
                    emit nodeDelayResult(result);
                    emit testProgress(current, total);
                }, Qt::QueuedConnection);
            });
            
            futures.append(future);
        }
        
        // 等待所有任务完成
        for (auto &future : futures) {
            future.waitForFinished();
        }
        
        // 发射完成信号
        QMetaObject::invokeMethod(this, [this]() {
            emit testCompleted();
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
