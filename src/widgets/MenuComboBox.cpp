#include "widgets/MenuComboBox.h"
#include "widgets/RoundedMenu.h"
#include "utils/ThemeManager.h"
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

    ThemeManager &tm = ThemeManager::instance();
    connect(&tm, &ThemeManager::themeChanged, this, [this]() {
        updateMenuStyle();
    });
}

void MenuComboBox::setWheelEnabled(bool enabled)
{
    m_wheelEnabled = enabled;
}

void MenuComboBox::showPopup()
{
    if (!m_menu) return;
    m_menu->clear();

    ThemeManager &tm = ThemeManager::instance();
    QColor checkColor = tm.getColor("primary");
    
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
    ThemeManager &tm = ThemeManager::instance();
    m_menu->setThemeColors(tm.getColor("bg-secondary"), tm.getColor("primary"));
    m_menu->setStyleSheet(QString(R"(
        #ComboMenu {
            background: transparent;
            border: none;
            padding: 6px;
        }
        #ComboMenu::panel {
            background: transparent;
            border: none;
        }
        #ComboMenu::item {
            color: %1;
            padding: 8px 14px;
            border-radius: 10px;
        }

        #ComboMenu::item:selected {
            background-color: %2;
        }
        #ComboMenu::item:selected:!checked {
            color: %1;
        }
        #ComboMenu::item:checked {
            color: %4;
        }
        #ComboMenu::item:checked:selected {
            color: %4;
        }
        #ComboMenu::separator {
            height: 1px;
            background-color: %3;
            margin: 6px 4px;
        }
    )")
    .arg(tm.getColorString("text-primary"))
    .arg(tm.getColorString("bg-tertiary"))
    .arg(tm.getColorString("border"))
    .arg(tm.getColorString("primary")));
}
