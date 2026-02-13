#ifndef GRIDCOMBOBOX_H
#define GRIDCOMBOBOX_H
#include <QComboBox>
class QListView;
class QPaintEvent;
class QWheelEvent;

class GridComboBox : public QComboBox {
  Q_OBJECT
 public:
  explicit GridComboBox(QWidget* parent = nullptr);

  void setWheelEnabled(bool enabled);
  bool isWheelEnabled() const { return m_wheelEnabled; }

  void setMaxColumns(int columns);
  void setVisibleRowRange(int minRows, int maxRows);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void showPopup() override;
  void wheelEvent(QWheelEvent* event) override;

 private:
  void updatePopupLayout();

  QListView* m_view           = nullptr;
  bool       m_wheelEnabled   = true;
  int        m_maxColumns     = 3;
  int        m_minVisibleRows = 4;
  int        m_maxVisibleRows = 9;
};

#endif  // GRIDCOMBOBOX_H
