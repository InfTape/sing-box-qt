#include "widgets/MenuComboBox.h"
#include "widgets/RoundedMenu.h"
#include "utils/ThemeManager.h"
#include <QAction>
#include <QPoint>
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

void MenuComboBox::showPopup()
{
    if (!m_menu) return;
    m_menu->clear();

    for (int i = 0; i < count(); ++i) {
        QAction *action = m_menu->addAction(itemText(i));
        action->setCheckable(true);
        action->setChecked(i == currentIndex());
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
        #ComboMenu::indicator {
            width: 14px;
            height: 14px;
        }
        #ComboMenu::indicator:checked {
            image: url(:/icons/check.svg);
        }
        #ComboMenu::indicator:unchecked {
            image: none;
        }
        #ComboMenu::item:selected {
            background-color: %2;
            color: white;
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
