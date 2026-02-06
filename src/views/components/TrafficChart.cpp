#include "TrafficChart.h"
#include <QDateTime>
#include <QPainterPath>
#include <cmath>
#include "app/interfaces/ThemeService.h"
TrafficChart::TrafficChart(ThemeService* themeService, QWidget* parent)
    : QWidget(parent), m_themeService(themeService), m_updateTimer(new QTimer(this)) {
  setMinimumHeight(150);
  // Initialize data arrays
  m_uploadData.fill(0, MAX_DATA_POINTS);
  m_downloadData.fill(0, MAX_DATA_POINTS);
  m_timeLabels.fill("", MAX_DATA_POINTS);
  // Setup update timer (1 second interval)
  connect(m_updateTimer, &QTimer::timeout, this, [this]() {
    // Shift data
    m_uploadData.removeFirst();
    m_downloadData.removeFirst();
    m_timeLabels.removeFirst();
    // Add new data point
    m_uploadData.append(m_lastUploadSpeed / 1024.0 / 1024.0);  // Convert to MB/s
    m_downloadData.append(m_lastDownloadSpeed / 1024.0 / 1024.0);
    // Add time label
    QDateTime now = QDateTime::currentDateTime();
    m_timeLabels.append(now.toString("mm:ss"));
    update();
  });
  m_updateTimer->start(1000);
  updateStyle();
  // Connect to theme changes
  if (m_themeService) {
    connect(m_themeService, &ThemeService::themeChanged, this, &TrafficChart::updateStyle);
  }
}
TrafficChart::~TrafficChart() {
  m_updateTimer->stop();
}
void TrafficChart::updateData(qint64 uploadSpeed, qint64 downloadSpeed) {
  m_lastUploadSpeed   = uploadSpeed;
  m_lastDownloadSpeed = downloadSpeed;
}
void TrafficChart::clear() {
  m_uploadData.fill(0, MAX_DATA_POINTS);
  m_downloadData.fill(0, MAX_DATA_POINTS);
  m_timeLabels.fill("", MAX_DATA_POINTS);
  m_lastUploadSpeed   = 0;
  m_lastDownloadSpeed = 0;
  update();
}
void TrafficChart::updateStyle() {
  ThemeService* ts = m_themeService;
  if (!ts) return;
  m_uploadColor   = ts->color("success");
  m_downloadColor = ts->color("primary");
  m_gridColor     = ts->color("border");
  m_textColor     = ts->color("text-secondary");
  m_bgColor       = ts->color("panel-bg");
  update();
}
void TrafficChart::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event)
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  // Draw background
  painter.fillRect(rect(), m_bgColor);
  drawChart(painter);
  drawLegend(painter);
}
void TrafficChart::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  update();
}
double TrafficChart::calculateMaxValue() const {
  double uploadMax   = 0.1;
  double downloadMax = 0.1;
  for (double val : m_uploadData) {
    if (val > uploadMax) uploadMax = val;
  }
  for (double val : m_downloadData) {
    if (val > downloadMax) downloadMax = val;
  }
  double currentMax = std::max(uploadMax, downloadMax);
  return std::max(currentMax * 1.2, 0.1);
}
void TrafficChart::drawChart(QPainter& painter) {
  const int paddingTop    = 24;
  const int paddingRight  = 15;
  const int paddingBottom = 28;
  const int paddingLeft   = 75;
  QRect chartRect(paddingLeft, paddingTop, width() - paddingLeft - paddingRight, height() - paddingTop - paddingBottom);
  drawGrid(painter, chartRect);
  // Draw curves
  if (m_uploadData.count(0) < MAX_DATA_POINTS) {
    drawCurve(painter, chartRect, m_uploadData, m_uploadColor);
  }
  if (m_downloadData.count(0) < MAX_DATA_POINTS) {
    drawCurve(painter, chartRect, m_downloadData, m_downloadColor);
  }
}
void TrafficChart::drawGrid(QPainter& painter, const QRect& chartRect) {
  double maxValue = calculateMaxValue();
  // Draw Y axis labels and grid lines
  painter.setFont(QFont(font().family(), 9));
  const int yAxisSteps = 4;
  for (int i = 0; i <= yAxisSteps; i++) {
    double y     = chartRect.bottom() - (static_cast<double>(i) / yAxisSteps) * chartRect.height();
    double value = (static_cast<double>(i) / yAxisSteps) * maxValue;
    // Grid line
    QPen gridPen(m_gridColor, 0.5, Qt::DashLine);
    painter.setPen(gridPen);
    painter.drawLine(QPointF(chartRect.left(), y), QPointF(chartRect.right(), y));
    // Y label
    painter.setPen(m_textColor);
    QString label = formatSpeed(value * 1024 * 1024);
    painter.drawText(QRectF(0, y - 10, chartRect.left() - 10, 20), Qt::AlignRight | Qt::AlignVCenter, label);
  }
  // Draw X axis
  QPen axisPen(m_gridColor, 0.8);
  painter.setPen(axisPen);
  painter.drawLine(chartRect.bottomLeft(), chartRect.bottomRight());
  // Draw X axis labels (every 12 points = ~12 seconds)
  painter.setFont(QFont(font().family(), 8));
  painter.setPen(m_textColor);
  const int labelInterval = MAX_DATA_POINTS / 5;
  for (int i = MAX_DATA_POINTS - 1; i >= 0; i -= labelInterval) {
    if (!m_timeLabels[i].isEmpty()) {
      double x = chartRect.left() + (static_cast<double>(i) / (MAX_DATA_POINTS - 1)) * chartRect.width();
      painter.drawText(QRectF(x - 25, chartRect.bottom() + 5, 50, 20), Qt::AlignCenter, m_timeLabels[i]);
    }
  }
}
void TrafficChart::drawCurve(QPainter& painter, const QRect& chartRect, const QVector<double>& data,
                             const QColor& color) {
  if (data.isEmpty()) return;
  double maxValue = calculateMaxValue();
  if (maxValue <= 0) maxValue = 0.1;
  // Build path using bezier curves
  QPainterPath path;
  auto         getPoint = [&](int i) -> QPointF {
    double x = chartRect.left() + (static_cast<double>(i) / (MAX_DATA_POINTS - 1)) * chartRect.width();
    double y = chartRect.bottom() - (data[i] / maxValue) * chartRect.height();
    return QPointF(x, y);
  };
  QPointF firstPoint = getPoint(0);
  path.moveTo(firstPoint);
  for (int i = 1; i < data.size(); i++) {
    QPointF curr = getPoint(i);
    QPointF prev = getPoint(i - 1);
    // Control points for smooth bezier curve
    double cpX1 = prev.x() + (curr.x() - prev.x()) / 3.0;
    double cpX2 = prev.x() + (curr.x() - prev.x()) * 2.0 / 3.0;
    path.cubicTo(QPointF(cpX1, prev.y()), QPointF(cpX2, curr.y()), curr);
  }
  // Draw line
  QPen linePen(color, 2.5);
  linePen.setJoinStyle(Qt::RoundJoin);
  linePen.setCapStyle(Qt::RoundCap);
  painter.setPen(linePen);
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(path);
  // Draw endpoint dot
  QPointF endPoint = getPoint(data.size() - 1);
  // Outer glow
  QColor glowColor = color;
  glowColor.setAlpha(64);
  painter.setBrush(glowColor);
  painter.setPen(Qt::NoPen);
  painter.drawEllipse(endPoint, 6, 6);
  // Inner dot
  painter.setBrush(color);
  painter.drawEllipse(endPoint, 3, 3);
}
void TrafficChart::drawLegend(QPainter& painter) {
  const int legendWidth  = 135;
  const int legendHeight = 22;
  const int margin       = 12;
  QRect     legendRect(width() - legendWidth - margin, margin, legendWidth, legendHeight);
  // Legend background
  painter.setPen(Qt::NoPen);
  QColor legendBg = m_bgColor;
  legendBg.setAlpha(220);
  painter.setBrush(legendBg);
  painter.drawRoundedRect(legendRect, 8, 8);
  // Legend items
  painter.setFont(QFont(font().family(), 9, QFont::DemiBold));
  int itemX   = legendRect.left() + 14;
  int centerY = legendRect.center().y();
  // Upload legend
  painter.setBrush(m_uploadColor);
  painter.setPen(Qt::NoPen);
  painter.drawEllipse(QPoint(itemX, centerY), 4, 4);
  painter.setPen(m_textColor);
  painter.drawText(QRect(itemX + 10, legendRect.top(), 50, legendHeight), Qt::AlignVCenter, tr("Up"));
  // Download legend
  itemX += 55;
  painter.setBrush(m_downloadColor);
  painter.setPen(Qt::NoPen);
  painter.drawEllipse(QPoint(itemX, centerY), 4, 4);
  painter.setPen(m_textColor);
  painter.drawText(QRect(itemX + 10, legendRect.top(), 50, legendHeight), Qt::AlignVCenter, tr("Down"));
}
QString TrafficChart::formatSpeed(double bytesPerSecond) const {
  const char* units[]   = {"B/s", "KB/s", "MB/s", "GB/s"};
  int         unitIndex = 0;
  double      size      = bytesPerSecond;
  while (size >= 1024 && unitIndex < 3) {
    size /= 1024;
    unitIndex++;
  }
  if (unitIndex == 0) {
    return QString::number(static_cast<int>(size)) + " " + units[unitIndex];
  }
  return QString::number(size, 'f', 0) + " " + units[unitIndex];
}
