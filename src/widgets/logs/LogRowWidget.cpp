#include "widgets/logs/LogRowWidget.h"
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>

LogRowWidget::LogRowWidget(const LogParser::LogEntry& entry, QWidget* parent)
    : QFrame(parent) {
  setObjectName("LogEntry");
  setProperty("logType", entry.type);
  QHBoxLayout* layout = new QHBoxLayout(this);
  layout->setContentsMargins(10, 6, 10, 6);
  layout->setSpacing(10);
  QLabel* timeLabel = new QLabel(entry.timestamp.toString("HH:mm:ss"));
  timeLabel->setObjectName("LogTime");
  const int badgePaddingX = 6;
  const int badgePaddingY = 2;
  QHBoxLayout* badgeLayout = new QHBoxLayout;
  badgeLayout->setContentsMargins(0, 0, 0, 0);
  badgeLayout->setSpacing(6);
  const auto addBadge = [&](const QString& text, const QString& type) {
    auto* badge = new QLabel(text);
    badge->setObjectName("LogBadge");
    badge->setProperty("logType", type);
    badge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    badge->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    const QSize size = QFontMetrics(badge->font()).size(Qt::TextSingleLine, text);
    badge->setFixedSize(size.width() + badgePaddingX * 2,
                        size.height() + badgePaddingY * 2);
    badgeLayout->addWidget(badge);
  };
  addBadge(LogParser::logTypeLabel(entry.type), entry.type);
  if (!entry.direction.isEmpty()) {
    QString directionLabel;
    if (entry.direction == "outbound") {
      directionLabel = tr("Outbound");
    } else if (entry.direction == "inbound") {
      directionLabel = tr("Inbound");
    } else if (entry.direction == "dns") {
      directionLabel = tr("DNS");
    } else {
      directionLabel = entry.direction.toUpper();
    }
    addBadge(directionLabel, "info");
  }
  if (entry.network == "tcp" || entry.network == "udp") {
    addBadge(entry.network.toUpper(), "info");
  }
  QWidget* badgeRow = new QWidget;
  badgeRow->setLayout(badgeLayout);
  QLabel* content =
      new QLabel(LogParser::stripSessionTracker(entry.payload));
  content->setObjectName("LogContent");
  content->setWordWrap(true);
  content->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  layout->addWidget(timeLabel, 0, Qt::AlignTop);
  layout->addWidget(badgeRow, 0, Qt::AlignTop);
  layout->addWidget(content, 1);
}
