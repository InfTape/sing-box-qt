#include "widgets/common/MenuComboBox.h"
#include "widgets/common/RoundedMenu.h"
#include "app/ThemeProvider.h"
#include <QAction>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPoint>
#include <QWheelEvent>
#include <QtMath>

MenuComboBox::MenuComboBox(QWidget *parent)
    : QComboBox(parent)
{
    m_menu = new RoundedMenu(this);
    m_menu->setObjectName("ComboMenu");
    updateMenuStyle();

    if (ThemeProvider::instance()) {
        connect(ThemeProvider::instance(), &ThemeService::themeChanged, this, [this]() {
            updateMenuStyle();
        });
    }
}

void MenuComboBox::setWheelEnabled(bool enabled)
{
    m_wheelEnabled = enabled;
}

void MenuComboBox::showPopup()
{
    if (!m_menu) return;
    m_menu->clear();

    ThemeService *ts = ThemeProvider::instance();
    QColor checkColor = ts ? ts->color("primary") : QColor(0, 0, 200);
    
    for (int i = 0; i < count(); ++i) {
        QAction *action = m_menu->addAction(itemText(i));
        
        if (i == currentIndex()) {
            // 为选中项创建勾号图标
            QPixmap pixmap(14, 14);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            
            QPen pen(checkColor, 2);
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            painter.setPen(pen);
            
            // 绘制勾号路径
            QPainterPath path;
            path.moveTo(1, 7);
            path.lineTo(5, 11);
            path.lineTo(13, 1);
            painter.drawPath(path);
            
            action->setIcon(QIcon(pixmap));
        }
        
        connect(action, &QAction::triggered, this, [this, i]() {
            setCurrentIndex(i);
        });
    }

    const int menuWidth = qMax(width(), 180);
    m_menu->setFixedWidth(menuWidth);
    m_menu->popup(mapToGlobal(QPoint(0, height())));
}

void MenuComboBox::hidePopup()
{
    if (m_menu) {
        m_menu->hide();
    }
}

void MenuComboBox::wheelEvent(QWheelEvent *event)
{
    if (!m_wheelEnabled) {
        event->ignore();
        return;
    }
    QComboBox::wheelEvent(event);
}

void MenuComboBox::updateMenuStyle()
{
    if (!m_menu) return;
    ThemeService *ts = ThemeProvider::instance();
    if (ts) m_menu->setThemeColors(ts->color("bg-secondary"), ts->color("primary"));
}
