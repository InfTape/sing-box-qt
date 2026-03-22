#include "widgets/common/ElideLabel.h"

#include <QEvent>
#include <QResizeEvent>
#include <QShowEvent>

ElideLabel::ElideLabel(QWidget* parent) : QLabel(parent) {}

void ElideLabel::setFullText(const QString& text) {
  if (m_fullText == text) {
    updateElidedText();
    return;
  }

  m_fullText = text;
  setToolTip(m_fullText);
  updateElidedText();
}

QString ElideLabel::fullText() const {
  return m_fullText;
}

void ElideLabel::setElideMode(Qt::TextElideMode mode) {
  if (m_elideMode == mode) {
    return;
  }

  m_elideMode = mode;
  updateElidedText();
}

void ElideLabel::resizeEvent(QResizeEvent* event) {
  QLabel::resizeEvent(event);
  updateElidedText();
}

void ElideLabel::showEvent(QShowEvent* event) {
  QLabel::showEvent(event);
  updateElidedText();
}

void ElideLabel::changeEvent(QEvent* event) {
  QLabel::changeEvent(event);
  if (event->type() == QEvent::FontChange) {
    updateElidedText();
  }
}

void ElideLabel::updateElidedText() {
  if (m_fullText.isEmpty()) {
    QLabel::clear();
    setToolTip(QString());
    return;
  }

  int availableWidth = contentsRect().width();
  if (availableWidth <= 0) {
    availableWidth = width();
  }

  if (availableWidth <= 0) {
    QLabel::setText(m_fullText);
    setToolTip(m_fullText);
    return;
  }

  const QString elided =
      fontMetrics().elidedText(m_fullText, m_elideMode, availableWidth);
  QLabel::setText(elided);
  setToolTip(m_fullText);
}
