#ifndef MENUCOMBOBOX_H
#define MENUCOMBOBOX_H

#include <QComboBox>

class RoundedMenu;

class MenuComboBox : public QComboBox
{
    Q_OBJECT

public:
    explicit MenuComboBox(QWidget *parent = nullptr);

protected:
    void showPopup() override;
    void hidePopup() override;

private:
    void updateMenuStyle();

    RoundedMenu *m_menu = nullptr;
};

#endif // MENUCOMBOBOX_H
