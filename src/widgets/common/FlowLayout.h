#ifndef FLOWLAYOUT_H
#define FLOWLAYOUT_H
#include <QLayout>
#include <QList>
#include <QStyle>

class FlowLayout : public QLayout {
 public:
  explicit FlowLayout(QWidget* parent = nullptr, int margin = -1, int hSpacing = -1, int vSpacing = -1);
  explicit FlowLayout(int margin, int hSpacing = -1, int vSpacing = -1);
  ~FlowLayout() override;
  void             addItem(QLayoutItem* item) override;
  int              horizontalSpacing() const;
  int              verticalSpacing() const;
  Qt::Orientations expandingDirections() const override;
  bool             hasHeightForWidth() const override;
  int              heightForWidth(int width) const override;
  int              count() const override;
  QLayoutItem*     itemAt(int index) const override;
  QLayoutItem*     takeAt(int index) override;
  QSize            minimumSize() const override;
  QSize            sizeHint() const override;
  void             setGeometry(const QRect& rect) override;

 private:
  int                 doLayout(const QRect& rect, bool testOnly) const;
  int                 smartSpacing(QStyle::PixelMetric pm) const;
  QList<QLayoutItem*> m_itemList;
  int                 m_hSpace;
  int                 m_vSpace;
};
#endif  // FLOWLAYOUT_H
