#ifndef TRAFFICCHART_H
#define TRAFFICCHART_H

#include <QPainter>
#include <QTimer>
#include <QVector>
#include <QWidget>

class ThemeService;
class TrafficChart : public QWidget {
  Q_OBJECT

 public:
  explicit TrafficChart(ThemeService* themeService = nullptr, QWidget* parent = nullptr);
  ~TrafficChart();

  void updateData(qint64 uploadSpeed, qint64 downloadSpeed);
  void clear();

 protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

 private:
  void    drawChart(QPainter& painter);
  void    drawGrid(QPainter& painter, const QRect& chartRect);
  void    drawCurve(QPainter& painter, const QRect& chartRect, const QVector<double>& data, const QColor& color);
  void    drawLegend(QPainter& painter);
  QString formatSpeed(double bytesPerSecond) const;
  double  calculateMaxValue() const;

  static const int MAX_DATA_POINTS = 60;

  QVector<double>  m_uploadData;
  QVector<double>  m_downloadData;
  QVector<QString> m_timeLabels;

  QColor        m_uploadColor;
  QColor        m_downloadColor;
  QColor        m_gridColor;
  QColor        m_textColor;
  QColor        m_bgColor;
  ThemeService* m_themeService = nullptr;

  QTimer* m_updateTimer;
  qint64  m_lastUploadSpeed   = 0;
  qint64  m_lastDownloadSpeed = 0;

 public slots:
  void updateStyle();
};
#endif  // TRAFFICCHART_H
