#ifndef DELAYTESTSERVICE_H
#define DELAYTESTSERVICE_H
#include <QFuture>
#include <QMutex>
#include <QObject>
#include <QSemaphore>
#include <QString>
#include <QVector>
#include <atomic>

// Delay test options.
struct DelayTestOptions {
  int     timeoutMs = 3000;  // Timeout in ms (Throne-dev default 3s).
  QString url;               // Test URL; empty uses AppSettings::urltestUrl().
  int     samples = 2;  // Sample count (Throne-dev uses 2 samples for re-test).
  int     concurrency =
      10;  // Concurrency (Throne-dev default 10, internal limit 100).
};

// Delay test result.
struct ProxyDelayTestResult {
  QString proxy;               // Proxy name.
  int     delay = 0;           // Delay in ms, 0 on failure.
  bool    ok    = false;       // Success flag.
  QString error;               // Error message.
  int     successSamples = 0;  // Successful samples.
};

class DelayTestService : public QObject {
  Q_OBJECT
 public:
  explicit DelayTestService(QObject* parent = nullptr);
  ~DelayTestService();
  // Set API port.
  void setApiPort(int port);
  void setApiToken(const QString& token);
  // Test single proxy delay (median of samples).
  void testNodeDelay(const QString&          proxy,
                     const DelayTestOptions& options = DelayTestOptions());
  // Test multiple proxies (with concurrency control).
  void testNodesDelay(const QStringList&      proxies,
                      const DelayTestOptions& options = DelayTestOptions());
  // Stop all tests.
  void stopAllTests();
  // Check if testing is running.
  bool isTesting() const;
 signals:
  // Single proxy test done.
  void nodeDelayResult(const ProxyDelayTestResult& result);
  // Batch progress.
  void testProgress(int current, int total);
  // Batch completed.
  void testCompleted();
  // Error happened.
  void errorOccurred(const QString& error);

 private:
  // Run a single delay test (internal).
  int fetchSingleDelay(const QString& proxy,
                       int            timeoutMs,
                       const QString& testUrl);
  // Compute median.
  int medianValue(QVector<int>& values);
  // Measure proxy delay with samples.
  ProxyDelayTestResult measureProxyDelay(const QString&          proxy,
                                         const DelayTestOptions& options);
  // Build Clash API URL.
  QString          buildClashDelayUrl(const QString& proxy,
                                      int            timeoutMs,
                                      const QString& testUrl) const;
  int              m_apiPort;
  bool             m_stopping;
  mutable QMutex   m_mutex;
  QSemaphore*      m_semaphore;   // Concurrency control.
  QFuture<void>    m_lastFuture;  // Hold last future to satisfy MSVC warning.
  QString          m_apiToken;
  std::atomic<int> m_activeTasks;
};
#endif  // DELAYTESTSERVICE_H
