#ifndef LOGTEXTSELECTION_H
#define LOGTEXTSELECTION_H
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QVector>

class QLabel;
class QScrollArea;
class QTimer;
class QWidget;

// A bounded selection across the separate log body labels, excluding badges.
class LogTextSelection : public QObject {
  Q_OBJECT
 public:
  explicit LogTextSelection(QScrollArea* area);
  void watchRow(QWidget* row);
  void clear();
  bool isActive() const;
  QString selectedText() const;
  void copy() const;
 signals:
  void changed();
 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
 private:
  struct Position {
    int row = -1;
    int offset = 0;
  };
  Position positionAt(const QPoint& global) const;
  void updateSelection();
  void selectAll();
  QScrollArea* m_area;
  QTimer* m_dragScroll;
  QVector<QPointer<QLabel>> m_labels;
  Position m_anchor;
  Position m_cursor;
  QPoint m_mouseGlobal;
  bool m_dragging = false;
};
#endif  // LOGTEXTSELECTION_H
