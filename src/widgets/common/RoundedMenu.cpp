#include "widgets/common/RoundedMenu.h"
#include "utils/ThemeManager.h"
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>

RoundedMenu::RoundedMenu(QWidget* parent)
    : QMenu(parent) {
  m_bgColor = ThemeManager::instance().getColor("panel-bg");
  m_borderColor = ThemeManager::instance().getColor("border-solid");
  setWindowFlags(windowFlags() | Qt::FramelessWindowHint |
                 Qt::NoDropShadowWindowHint);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setAttribute(Qt::WA_NoSystemBackground, true);
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
    m_bgColor = ThemeManager::instance().getColor("panel-bg");
    m_borderColor = ThemeManager::instance().getColor("border-solid");
    update();
  });
}

void RoundedMenu::setThemeColors(const QColor& bg, const QColor& border) {
  m_bgColor     = bg;
  m_borderColor = border;
  update();
}

void RoundedMenu::paintEvent(QPaintEvent* event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  QRectF       rect = this->rect().adjusted(1, 1, -1, -1);
  QPainterPath path;
  path.addRoundedRect(rect, 10, 10);
  painter.fillPath(path, m_bgColor);
  painter.setPen(QPen(m_borderColor, 1));
  painter.drawPath(path);
  QMenu::paintEvent(event);
}
