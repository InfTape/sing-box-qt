#ifndef ELIDELABEL_H
#define ELIDELABEL_H

#include <QLabel>

class QResizeEvent;
class QShowEvent;

class ElideLabel : public QLabel {
  Q_OBJECT

 public:
  explicit ElideLabel(QWidget* parent = nullptr);

  void    setFullText(const QString& text);
  QString fullText() const;
  void    setElideMode(Qt::TextElideMode mode);

 protected:
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void changeEvent(QEvent* event) override;

 private:
  void updateElidedText();

  QString           m_fullText;
  Qt::TextElideMode m_elideMode = Qt::ElideRight;
};

#endif  // ELIDELABEL_H
