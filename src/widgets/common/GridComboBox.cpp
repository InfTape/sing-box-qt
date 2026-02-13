#include "widgets/common/GridComboBox.h"
#include <QGuiApplication>
#include <QFrame>
#include <QListView>
#include <QPoint>
#include <QProxyStyle>
#include <QScreen>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QStylePainter>
#include <QWheelEvent>

namespace {
constexpr int kPopupHorizontalPadding = 12;
constexpr int kPopupVerticalPadding   = 12;
constexpr int kMinCellWidth           = 180;
constexpr int kMaxCellWidth           = 240;
constexpr int kCellTextPadding        = 28;
constexpr int kPopupScreenMargin      = 24;
constexpr int kPopupMaxWidth          = 760;

class GridComboStyle : public QProxyStyle {
 public:
  explicit GridComboStyle(QStyle* baseStyle) : QProxyStyle(baseStyle) {}

  int styleHint(StyleHint       hint,
                const QStyleOption* option,
                const QWidget*      widget,
                QStyleHintReturn*   returnData) const override {
    if (hint == QStyle::SH_ComboBox_Popup) {
      return 0;
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
  }
};
}  // namespace

GridComboBox::GridComboBox(QWidget* parent) : QComboBox(parent) {
  auto* styleProxy = new GridComboStyle(style());
  styleProxy->setParent(this);
  setStyle(styleProxy);

  m_view = new QListView(this);
  m_view->setObjectName("GridComboPopup");
  m_view->setViewMode(QListView::IconMode);
  m_view->setFlow(QListView::LeftToRight);
  m_view->setWrapping(true);
  m_view->setMovement(QListView::Static);
  m_view->setResizeMode(QListView::Adjust);
  m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_view->setTextElideMode(Qt::ElideRight);
  m_view->setWordWrap(false);
  m_view->setUniformItemSizes(true);
  m_view->setFrameShape(QFrame::NoFrame);
  m_view->setMouseTracking(true);
  m_view->setSpacing(4);
  setView(m_view);
}

void GridComboBox::setWheelEnabled(bool enabled) {
  m_wheelEnabled = enabled;
}

void GridComboBox::setMaxColumns(int columns) {
  m_maxColumns = qMax(1, columns);
}

void GridComboBox::setVisibleRowRange(int minRows, int maxRows) {
  m_minVisibleRows = qMax(1, minRows);
  m_maxVisibleRows = qMax(m_minVisibleRows, maxRows);
}

void GridComboBox::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  QStylePainter        painter(this);
  QStyleOptionComboBox opt;
  initStyleOption(&opt);
  const QRect textRect = style()->subControlRect(
      QStyle::CC_ComboBox, &opt, QStyle::SC_ComboBoxEditField, this);
  opt.currentText = fontMetrics().elidedText(
      opt.currentText, Qt::ElideRight, qMax(0, textRect.width()));
  painter.drawComplexControl(QStyle::CC_ComboBox, opt);
  painter.drawControl(QStyle::CE_ComboBoxLabel, opt);
}

void GridComboBox::showPopup() {
  updatePopupLayout();
  QComboBox::showPopup();
  // Qt may recreate/reposition popup after show; apply once more.
  updatePopupLayout();
}

void GridComboBox::wheelEvent(QWheelEvent* event) {
  if (!m_wheelEnabled) {
    event->ignore();
    return;
  }
  QComboBox::wheelEvent(event);
}

void GridComboBox::updatePopupLayout() {
  if (!m_view) {
    return;
  }
  QScreen* screen = QGuiApplication::screenAt(
      mapToGlobal(QPoint(width() / 2, height() / 2)));
  if (!screen) {
    screen = QGuiApplication::primaryScreen();
  }

  QRect available(0, 0, 1280, 720);
  if (screen) {
    available = screen->availableGeometry();
  }

  int longestText = 0;
  for (int i = 0; i < count(); ++i) {
    longestText = qMax(longestText, fontMetrics().horizontalAdvance(itemText(i)));
  }
  const int rowHeight = qMax(34, fontMetrics().height() + 14);
  const int cellWidth =
      qBound(kMinCellWidth, longestText + kCellTextPadding, kMaxCellWidth);

  const int maxPopupWidth = qMax(
      width(),
      qMin(available.width() - kPopupScreenMargin, kPopupMaxWidth));
  const int maxColumnsByWidth = qMax(1, maxPopupWidth / cellWidth);
  int       columns = qMin(m_maxColumns, maxColumnsByWidth);
  columns           = qMin(columns, qMax(1, count()));
  columns           = qMax(1, columns);

  int popupWidth = qMax(
      width(), columns * cellWidth + kPopupHorizontalPadding * 2);
  popupWidth = qMin(popupWidth, maxPopupWidth);
  const int adjustedCellWidth =
      qMax(kMinCellWidth, (popupWidth - kPopupHorizontalPadding * 2) / columns);
  m_view->setGridSize(QSize(adjustedCellWidth, rowHeight));

  const int rowCount =
      qMax(1, (count() + columns - 1) / columns);
  const int visibleRows = qBound(m_minVisibleRows, rowCount, m_maxVisibleRows);
  int popupHeight =
      visibleRows * rowHeight + kPopupVerticalPadding * 2;
  const int maxPopupHeight = qMax(
      rowHeight * 2, available.height() - kPopupScreenMargin);
  popupHeight = qMin(popupHeight, maxPopupHeight);

  m_view->setMinimumWidth(popupWidth);
  m_view->setMaximumWidth(popupWidth);
  m_view->setMinimumHeight(popupHeight);
  m_view->setMaximumHeight(maxPopupHeight);

  if (QWidget* popup = m_view->window()) {
    popup->setObjectName("GridComboPopupWindow");
    popup->setContentsMargins(0, 0, 0, 0);
    if (QFrame* frame = qobject_cast<QFrame*>(popup)) {
      frame->setFrameShape(QFrame::NoFrame);
      frame->setLineWidth(0);
    }
    popup->setMinimumWidth(popupWidth);
    popup->setMaximumWidth(popupWidth);
    popup->setMinimumHeight(popupHeight);
    popup->setMaximumHeight(maxPopupHeight);

    const QPoint comboTopLeft = mapToGlobal(QPoint(0, 0));
    int          x            = comboTopLeft.x();
    int          y            = comboTopLeft.y() + height();
    const int rightExclusive  = available.right() + 1;
    const int bottomExclusive = available.bottom() + 1;

    if (x + popupWidth > rightExclusive) {
      x = rightExclusive - popupWidth;
    }
    if (x < available.left()) {
      x = available.left();
    }
    if (y + popupHeight > bottomExclusive) {
      const int aboveY = comboTopLeft.y() - popupHeight;
      if (aboveY >= available.top()) {
        y = aboveY;
      } else {
        y = qMax(available.top(), bottomExclusive - popupHeight);
      }
    }
    if (y < available.top()) {
      y = available.top();
    }

    popup->setGeometry(x, y, popupWidth, popupHeight);
  }
}
