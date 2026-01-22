#ifndef DELAYTESTSERVICE_H
#define DELAYTESTSERVICE_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QMutex>
#include <QSemaphore>

class HttpClient;

// 延迟测试配置选项
struct DelayTestOptions {
    int timeoutMs = 8000;                                              // 超时时间（毫秒）
    QString url = "https://connectivitycheck.gstatic.com/generate_204"; // 测试 URL
    int samples = 2;                                                    // 采样次数
    int concurrency = 6;                                                // 并发数
};

// 延迟测试结果
struct ProxyDelayTestResult {
    QString proxy;         // 节点名称
    int delay = 0;         // 延迟（ms），失败时为0
    bool ok = false;       // 是否成功
    QString error;         // 错误信息
    int successSamples = 0; // 成功采样数
};

class DelayTestService : public QObject
{
    Q_OBJECT

public:
    explicit DelayTestService(QObject *parent = nullptr);
    ~DelayTestService();

    // 设置 API 端口
    void setApiPort(int port);
    
    // 测试单个节点延迟（多采样取中位数）
    void testNodeDelay(const QString &proxy, const DelayTestOptions &options = DelayTestOptions());
    
    // 测试多个节点延迟（并发控制）
    void testNodesDelay(const QStringList &proxies, const DelayTestOptions &options = DelayTestOptions());
    
    // 测试代理组延迟
    void testGroupDelay(const QString &group, const QJsonArray &nodes, const DelayTestOptions &options = DelayTestOptions());
    
    // 停止所有测试
    void stopAllTests();
    
    // 是否正在测试
    bool isTesting() const;

signals:
    // 单个节点测试完成
    void nodeDelayResult(const ProxyDelayTestResult &result);
    
    // 批量测试进度
    void testProgress(int current, int total);
    
    // 批量测试完成
    void testCompleted();
    
    // 发生错误
    void errorOccurred(const QString &error);

private:
    // 执行单次延迟测试（内部使用）
    int fetchSingleDelay(const QString &proxy, int timeoutMs, const QString &testUrl);
    
    // 计算中位数
    int medianValue(QVector<int> &values);
    
    // 测量节点延迟（多采样）
    ProxyDelayTestResult measureProxyDelay(const QString &proxy, const DelayTestOptions &options);
    
    // 构建 Clash API URL
    QString buildClashDelayUrl(const QString &proxy, int timeoutMs, const QString &testUrl) const;

    HttpClient *m_httpClient;
    int m_apiPort;
    bool m_stopping;
    mutable QMutex m_mutex;
    QSemaphore *m_semaphore;  // 并发控制信号量
};

#endif // DELAYTESTSERVICE_H
