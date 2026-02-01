#ifndef CHEVRON_TOGGLE_H
#define CHEVRON_TOGGLE_H

#include <QWidget>

class QPropertyAnimation;

class ChevronToggle : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool expanded READ isExpanded WRITE setExpanded NOTIFY expandedChanged)
    Q_PROPERTY(qreal progress READ progress WRITE setProgress)

public:
    explicit ChevronToggle(QWidget *parent = nullptr);

    bool isExpanded() const { return m_expanded; }
    void setExpanded(bool expanded);

    qreal progress() const { return m_progress; }
    void setProgress(qreal value);

signals:
    void expandedChanged(bool expanded);
    void toggled(bool expanded);
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    QSize sizeHint() const override;

private:
    bool m_expanded = false;
    qreal m_progress = 0.0;
    QPropertyAnimation *m_anim = nullptr;
};

#endif // CHEVRON_TOGGLE_H
