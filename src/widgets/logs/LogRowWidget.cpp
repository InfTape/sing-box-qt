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

  QLabel* typeLabel = new QLabel(LogParser::logTypeLabel(entry.type));
  typeLabel->setObjectName("LogBadge");
  typeLabel->setProperty("logType", entry.type);
  typeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  typeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  const QFontMetrics typeMetrics(typeLabel->font());
  const QSize        typeSize =
      typeMetrics.size(Qt::TextSingleLine, typeLabel->text());
  typeLabel->setFixedSize(typeSize.width() + badgePaddingX * 2,
                          typeSize.height() + badgePaddingY * 2);

  QHBoxLayout* badgeLayout = new QHBoxLayout;
  badgeLayout->setContentsMargins(0, 0, 0, 0);
  badgeLayout->setSpacing(6);
  badgeLayout->addWidget(typeLabel);

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
    QLabel* directionTag = new QLabel(directionLabel);
    directionTag->setObjectName("LogBadge");
    directionTag->setProperty("logType", "info");
    directionTag->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    directionTag->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    const QFontMetrics dirMetrics(directionTag->font());
    const QSize        dirSize =
        dirMetrics.size(Qt::TextSingleLine, directionTag->text());
    directionTag->setFixedSize(dirSize.width() + badgePaddingX * 2,
                               dirSize.height() + badgePaddingY * 2);
    badgeLayout->addWidget(directionTag);
  }

  QWidget* badgeRow = new QWidget;
  badgeRow->setLayout(badgeLayout);

  QLabel* content = new QLabel(entry.payload);
  content->setObjectName("LogContent");
  content->setWordWrap(true);
  content->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

  layout->addWidget(timeLabel, 0, Qt::AlignTop);
  layout->addWidget(badgeRow, 0, Qt::AlignTop);
  layout->addWidget(content, 1);
}
