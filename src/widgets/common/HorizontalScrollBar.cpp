#include "HorizontalScrollBar.h"

HorizontalScrollBar::HorizontalScrollBar(QWidget* parent)
    : QScrollBar(Qt::Horizontal, parent) {
  // Both orientations share the scrollbar rules in the global theme stylesheet.
}
