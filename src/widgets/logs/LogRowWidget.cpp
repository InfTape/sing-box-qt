#include "widgets/logs/LogRowWidget.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>

LogRowWidget::LogRowWidget(const LogParser::LogEntry& entry, QWidget* parent)
    : QFrame(parent) {
  setObjectName("LogEntry");
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(10, 6, 10, 6);
  layout->setSpacing(10);
  m_time = new QLabel(this);
  m_time->setObjectName("LogTime");
  auto* badgeRow = new QWidget(this);
  auto* badgeLayout = new QHBoxLayout(badgeRow);
  badgeLayout->setContentsMargins(0, 0, 0, 0);
  badgeLayout->setSpacing(6);
  const auto createBadge = [&]() {
    auto* badge = new QLabel(badgeRow);
    badge->setObjectName("LogBadge");
    badge->setProperty("logType", "info");
    badge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    badge->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    badgeLayout->addWidget(badge);
    return badge;
  };
  m_level = createBadge();
  m_direction = createBadge();
  m_network = createBadge();
  m_content = new QLabel(this);
  m_content->setObjectName("LogContent");
  m_content->setTextFormat(Qt::PlainText);
  m_content->setWordWrap(true);
  m_content->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  m_content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  layout->addWidget(m_time, 0, Qt::AlignTop);
  layout->addWidget(badgeRow, 0, Qt::AlignTop);
  layout->addWidget(m_content, 1);
  setEntry(entry);
}

void LogRowWidget::setBadge(QLabel* badge, const QString& text) {
  badge->setText(text);
  badge->setVisible(!text.isEmpty());
  // The fixed size policy uses QLabel's style-aware size hint, including
  // scaled padding and text margins, and updates when pooled text changes.
}

void LogRowWidget::setEntry(const LogParser::LogEntry& entry) {
  if (m_type != entry.type) {
    m_type = entry.type;
    setProperty("logType", m_type);
    m_level->setProperty("logType", m_type);
    // Re-evaluate both badge and descendant content styles on a level change.
    for (QWidget* widget : {static_cast<QWidget*>(this),
                            static_cast<QWidget*>(m_level),
                            static_cast<QWidget*>(m_content)}) {
      widget->style()->unpolish(widget);
      widget->style()->polish(widget);
      widget->update();
    }
  }
  m_time->setText(entry.timestamp.toString("HH:mm:ss"));
  setBadge(m_level, LogParser::logTypeLabel(entry.type));
  QString direction;
  if (entry.direction == "inbound") direction = tr("Inbound");
  else if (entry.direction == "outbound") direction = tr("Outbound");
  else if (entry.direction == "dns") direction = tr("DNS");
  else direction = entry.direction.toUpper();
  setBadge(m_direction, direction);
  setBadge(m_network, entry.network == "tcp" || entry.network == "udp"
                          ? entry.network.toUpper() : QString());
  m_content->setSelection(0, 0);
  m_content->setText(LogParser::stripSessionTracker(entry.payload));
}

void LogRowWidget::reset() {
  hide();
  m_time->clear();
  // setText preserves QLabel's text control; clear() destroys it. Populate the
  // empty document now so hidden pool entries release their previous payload.
  m_content->setText(QStringLiteral(""));
  m_content->setSelection(0, 0);
  for (auto* badge : {m_level, m_direction, m_network}) {
    badge->clear();
    badge->hide();
  }
}
