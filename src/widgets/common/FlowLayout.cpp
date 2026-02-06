#include "FlowLayout.h"
#include <QLayoutItem>
#include <QSizePolicy>
#include <QStyle>
#include <QWidget>

FlowLayout::FlowLayout(QWidget* parent, int margin, int hSpacing, int vSpacing)
    : QLayout(parent), m_hSpace(hSpacing), m_vSpace(vSpacing) {
  if (margin >= 0) {
    setContentsMargins(margin, margin, margin, margin);
  }
}

FlowLayout::FlowLayout(int margin, int hSpacing, int vSpacing)
    : m_hSpace(hSpacing), m_vSpace(vSpacing) {
  if (margin >= 0) {
    setContentsMargins(margin, margin, margin, margin);
  }
}

FlowLayout::~FlowLayout() {
  QLayoutItem* item = nullptr;
  while ((item = takeAt(0)) != nullptr) {
    delete item;
  }
}

void FlowLayout::addItem(QLayoutItem* item) {
  m_itemList.append(item);
}

int FlowLayout::horizontalSpacing() const {
  if (m_hSpace >= 0) {
    return m_hSpace;
  }
  return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
}

int FlowLayout::verticalSpacing() const {
  if (m_vSpace >= 0) {
    return m_vSpace;
  }
  return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
}

Qt::Orientations FlowLayout::expandingDirections() const {
  return {};
}

bool FlowLayout::hasHeightForWidth() const {
  return true;
}

int FlowLayout::heightForWidth(int width) const {
  return doLayout(QRect(0, 0, width, 0), true);
}

int FlowLayout::count() const {
  return m_itemList.size();
}

QLayoutItem* FlowLayout::itemAt(int index) const {
  return m_itemList.value(index);
}

QLayoutItem* FlowLayout::takeAt(int index) {
  if (index < 0 || index >= m_itemList.size()) {
    return nullptr;
  }
  return m_itemList.takeAt(index);
}

QSize FlowLayout::minimumSize() const {
  QSize size;
  for (QLayoutItem* item : m_itemList) {
    size = size.expandedTo(item->minimumSize());
  }
  int left   = 0;
  int top    = 0;
  int right  = 0;
  int bottom = 0;
  getContentsMargins(&left, &top, &right, &bottom);
  size += QSize(left + right, top + bottom);
  return size;
}

QSize FlowLayout::sizeHint() const {
  return minimumSize();
}

void FlowLayout::setGeometry(const QRect& rect) {
  QLayout::setGeometry(rect);
  doLayout(rect, false);
}

int FlowLayout::doLayout(const QRect& rect, bool testOnly) const {
  int left   = 0;
  int top    = 0;
  int right  = 0;
  int bottom = 0;
  getContentsMargins(&left, &top, &right, &bottom);
  const QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
  int         x             = effectiveRect.x();
  int         y             = effectiveRect.y();
  int         lineHeight    = 0;
  for (QLayoutItem* item : m_itemList) {
    QWidget* itemWidget = item->widget();
    int      spaceX     = horizontalSpacing();
    if (spaceX < 0) {
      spaceX = itemWidget
                   ? itemWidget->style()->layoutSpacing(QSizePolicy::PushButton,
                                                        QSizePolicy::PushButton,
                                                        Qt::Horizontal)
                   : 0;
    }
    int spaceY = verticalSpacing();
    if (spaceY < 0) {
      spaceY = itemWidget
                   ? itemWidget->style()->layoutSpacing(QSizePolicy::PushButton,
                                                        QSizePolicy::PushButton,
                                                        Qt::Vertical)
                   : 0;
    }
    const QSize itemSize = item->sizeHint();
    int         nextX    = x + itemSize.width() + spaceX;
    if (nextX - spaceX > effectiveRect.right() + 1 && lineHeight > 0) {
      x          = effectiveRect.x();
      y          = y + lineHeight + spaceY;
      nextX      = x + itemSize.width() + spaceX;
      lineHeight = 0;
    }
    if (!testOnly) {
      item->setGeometry(QRect(QPoint(x, y), itemSize));
    }
    x          = nextX;
    lineHeight = qMax(lineHeight, itemSize.height());
  }
  return y + lineHeight - rect.y() + bottom;
}

int FlowLayout::smartSpacing(QStyle::PixelMetric pm) const {
  QObject* parentObj = parent();
  if (!parentObj) {
    return -1;
  }
  if (parentObj->isWidgetType()) {
    QWidget* parentWidget = static_cast<QWidget*>(parentObj);
    return parentWidget->style()->pixelMetric(pm, nullptr, parentWidget);
  }
  return static_cast<QLayout*>(parentObj)->spacing();
}
