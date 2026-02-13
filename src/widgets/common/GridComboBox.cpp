#include "widgets/common/GridComboBox.h"
#include <QFrame>
#include <QGuiApplication>
#include <QListWidget>
#include <QPoint>
#include <QScreen>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QStylePainter>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QWheelEvent>
#include "app/interfaces/ThemeService.h"
#include "widgets/common/RoundedMenu.h"

namespace {
constexpr int kPopupPadding        = 8;
constexpr int kMinCellWidth        = 180;
constexpr int kMaxCellWidth        = 240;
constexpr int kCellTextPadding     = 28;
constexpr int kPopupScreenMargin   = 24;
constexpr int kPopupMaxWidth       = 760;
constexpr int kMinimumItemRowHeight = 36;
}  // namespace

GridComboBox::GridComboBox(QWidget* parent, ThemeService* themeService)
    : QComboBox(parent), m_themeService(themeService) {
  m_menu = new RoundedMenu(this);
  m_menu->setObjectName("GridComboMenu");

  m_popupContent = new QWidget(m_menu);
  m_popupContent->setObjectName("GridComboContent");
  auto* contentLayout = new QVBoxLayout(m_popupContent);
  contentLayout->setContentsMargins(
      kPopupPadding, kPopupPadding, kPopupPadding, kPopupPadding);
  contentLayout->setSpacing(0);

  m_listWidget = new QListWidget(m_popupContent);
  m_listWidget->setObjectName("GridComboList");
  m_listWidget->setViewMode(QListView::IconMode);
  m_listWidget->setFlow(QListView::LeftToRight);
  m_listWidget->setWrapping(true);
  m_listWidget->setMovement(QListView::Static);
  m_listWidget->setResizeMode(QListView::Adjust);
  m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_listWidget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_listWidget->setTextElideMode(Qt::ElideRight);
  m_listWidget->setWordWrap(false);
  m_listWidget->setUniformItemSizes(true);
  m_listWidget->setFrameShape(QFrame::NoFrame);
  m_listWidget->setMouseTracking(true);
  m_listWidget->setSpacing(4);
  contentLayout->addWidget(m_listWidget);

  auto* action = new QWidgetAction(m_menu);
  action->setDefaultWidget(m_popupContent);
  m_menu->addAction(action);

  auto onPick = [this](QListWidgetItem* item) {
    if (!item) {
      return;
    }
    const int idx = item->data(Qt::UserRole).toInt();
    if (idx >= 0 && idx < count()) {
      setCurrentIndex(idx);
    }
    if (m_menu) {
      m_menu->hide();
    }
  };
  connect(m_listWidget, &QListWidget::itemClicked, this, onPick);
  connect(m_listWidget, &QListWidget::itemActivated, this, onPick);

  updateMenuStyle();
  if (m_themeService) {
    connect(m_themeService,
            &ThemeService::themeChanged,
            this,
            &GridComboBox::updateMenuStyle);
  }
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

void GridComboBox::setThemeService(ThemeService* themeService) {
  if (m_themeService == themeService) {
    return;
  }
  if (m_themeService) {
    disconnect(m_themeService, nullptr, this, nullptr);
  }
  m_themeService = themeService;
  if (m_themeService) {
    connect(m_themeService,
            &ThemeService::themeChanged,
            this,
            &GridComboBox::updateMenuStyle);
  }
  updateMenuStyle();
}

void GridComboBox::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  QStylePainter        painter(this);
  QStyleOptionComboBox opt;
  initStyleOption(&opt);
  opt.state &= ~QStyle::State_Sunken;
  const QRect textRect = style()->subControlRect(
      QStyle::CC_ComboBox, &opt, QStyle::SC_ComboBoxEditField, this);
  opt.currentText = fontMetrics().elidedText(
      opt.currentText, Qt::ElideRight, qMax(0, textRect.width()));
  painter.drawComplexControl(QStyle::CC_ComboBox, opt);
  painter.drawControl(QStyle::CE_ComboBoxLabel, opt);
}

void GridComboBox::showPopup() {
  if (!m_menu || !m_listWidget) {
    return;
  }
  m_listWidget->clear();
  for (int i = 0; i < count(); ++i) {
    auto* item = new QListWidgetItem(itemText(i), m_listWidget);
    item->setData(Qt::UserRole, i);
    item->setTextAlignment(Qt::AlignCenter);
    if (i == currentIndex()) {
      m_listWidget->setCurrentItem(item);
    }
  }
  updatePopupLayout();
  m_menu->popup(mapToGlobal(QPoint(0, height())));
}

void GridComboBox::hidePopup() {
  if (m_menu) {
    m_menu->hide();
  }
}

void GridComboBox::wheelEvent(QWheelEvent* event) {
  if (!m_wheelEnabled) {
    event->ignore();
    return;
  }
  QComboBox::wheelEvent(event);
}

void GridComboBox::updatePopupLayout() {
  if (!m_menu || !m_listWidget || !m_popupContent) {
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
  const int rowHeight = qMax(
      kMinimumItemRowHeight, fontMetrics().height() + 16);
  const int cellWidth =
      qBound(kMinCellWidth, longestText + kCellTextPadding, kMaxCellWidth);

  const int maxPopupWidth = qMax(
      width(),
      qMin(available.width() - kPopupScreenMargin, kPopupMaxWidth));
  const int maxColumnsByWidth =
      qMax(1, (maxPopupWidth - kPopupPadding * 2) / cellWidth);
  int columns = qMin(m_maxColumns, maxColumnsByWidth);
  columns     = qMin(columns, qMax(1, count()));
  columns     = qMax(1, columns);

  int listWidth = qMax(width(), columns * cellWidth);
  listWidth = qMin(listWidth, maxPopupWidth - kPopupPadding * 2);

  const int adjustedCellWidth = qMax(kMinCellWidth, listWidth / columns);
  m_listWidget->setGridSize(QSize(adjustedCellWidth, rowHeight));

  const int rowCount    = qMax(1, (count() + columns - 1) / columns);
  const int visibleRows = qBound(m_minVisibleRows, rowCount, m_maxVisibleRows);
  const bool needScroll = rowCount > visibleRows;
  if (needScroll) {
    const int scrollExtent = m_listWidget->style()->pixelMetric(
        QStyle::PM_ScrollBarExtent, nullptr, m_listWidget);
    listWidth += scrollExtent + 2;
  }

  int listHeight = visibleRows * rowHeight;
  const int maxPopupHeight = qMax(
      rowHeight * 2, available.height() - kPopupScreenMargin);
  int popupHeight = qMin(maxPopupHeight, listHeight + kPopupPadding * 2);
  listHeight      = qMax(rowHeight * 2, popupHeight - kPopupPadding * 2);

  const int popupWidth = listWidth + kPopupPadding * 2;
  m_listWidget->setFixedSize(listWidth, listHeight);
  m_popupContent->setFixedSize(popupWidth, popupHeight);
  m_menu->setFixedSize(popupWidth, popupHeight);
}

void GridComboBox::updateMenuStyle() {
  if (!m_menu) {
    return;
  }
  if (m_themeService) {
    m_menu->setThemeColors(m_themeService->color("bg-secondary"),
                           m_themeService->color("primary"));
    return;
  }
  m_menu->setThemeColors(
      palette().color(QPalette::Base), palette().color(QPalette::Mid));
}
