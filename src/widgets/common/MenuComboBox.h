#ifndef MENUCOMBOBOX_H
#define MENUCOMBOBOX_H

#include <QComboBox>

class QWheelEvent;

class RoundedMenu;

class MenuComboBox : public QComboBox
{
    Q_OBJECT

public:
    explicit MenuComboBox(QWidget *parent = nullptr);
    void setWheelEnabled(bool enabled);
    bool isWheelEnabled() const { return m_wheelEnabled; }

protected:
    void showPopup() override;
    void hidePopup() override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void updateMenuStyle();

    RoundedMenu *m_menu = nullptr;
    bool m_wheelEnabled = true;
};

#endif // MENUCOMBOBOX_H
