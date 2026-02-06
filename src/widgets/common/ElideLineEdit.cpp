#include "widgets/common/ElideLineEdit.h"
#include <QPaintEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionFrame>

ElideLineEdit::ElideLineEdit(QWidget* parent) : QLineEdit(parent) {}

void ElideLineEdit::setElideMode(Qt::TextElideMode mode) {
  if (m_elideMode == mode) {
    return;
  }
  m_elideMode = mode;
  update();
}

void ElideLineEdit::paintEvent(QPaintEvent* event) {
  if (hasFocus() || text().isEmpty()) {
    QLineEdit::paintEvent(event);
    return;
  }
  QStyleOptionFrame panel;
  initStyleOption(&panel);
  QPainter painter(this);
  style()->drawPrimitive(QStyle::PE_PanelLineEdit, &panel, &painter, this);
  QRect textRect =
      style()->subElementRect(QStyle::SE_LineEditContents, &panel, this);
  const QString elided =
      fontMetrics().elidedText(text(), m_elideMode, qMax(0, textRect.width()));
  painter.setPen(palette().color(QPalette::Text));
  painter.setFont(font());
  painter.drawText(textRect, alignment() | Qt::AlignVCenter, elided);
}
