#ifndef LOGGER_H
#define LOGGER_H
#include <QFile>
#include <QMutex>
#include <QString>
class Logger {
 public:
  static Logger& instance();
  void           init();
  void           close();
  static void    debug(const QString& message);
  static void    info(const QString& message);
  static void    warn(const QString& message);
  static void    error(const QString& message);

 private:
  Logger();
  ~Logger();
  void    log(const QString& level, const QString& message);
  QString getLogFilePath() const;
  QFile   m_logFile;
  QMutex  m_mutex;
  bool    m_initialized;
};
#endif  // LOGGER_H
