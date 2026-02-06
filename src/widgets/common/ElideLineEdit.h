#ifndef ELIDELINEEDIT_H
#define ELIDELINEEDIT_H

#include <QLineEdit>

class QPaintEvent;

class ElideLineEdit : public QLineEdit {
  Q_OBJECT

 public:
  explicit ElideLineEdit(QWidget* parent = nullptr);
  void setElideMode(Qt::TextElideMode mode);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  Qt::TextElideMode m_elideMode = Qt::ElideRight;
};

#endif  // ELIDELINEEDIT_H
