#ifndef TOASTNOTIFICATION_H
#define TOASTNOTIFICATION_H

#include <QTimer>
#include <QWidget>

class ThemeService;

class ToastNotification : public QWidget {
  Q_OBJECT
  Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)
  Q_PROPERTY(int   offsetY READ offsetY WRITE setOffsetY)

 public:
  enum class Tone { Primary, Error };

  static void show(QWidget*       parent,
                   const QString& message,
                   ThemeService*  theme      = nullptr,
                   int            durationMs = 2500,
                   Tone           tone       = Tone::Primary);

  explicit ToastNotification(QWidget*       parent,
                             const QString& message,
                             ThemeService*  theme,
                             int            durationMs,
                             Tone           tone);
  ~ToastNotification() override = default;

  qreal opacity() const;
  void  setOpacity(qreal value);
  int   offsetY() const;
  void  setOffsetY(int y);

 protected:
  void paintEvent(QPaintEvent* event) override;
  bool eventFilter(QObject* obj, QEvent* event) override;

 private:
  void startShowAnimation();
  void startHideAnimation();
  void reposition();

  QString       m_message;
  ThemeService* m_theme    = nullptr;
  Tone          m_tone     = Tone::Primary;
  qreal         m_opacity  = 0.0;
  int           m_offsetY  = 0;
  int           m_duration = 2500;
  QTimer        m_hideTimer;
};

#endif  // TOASTNOTIFICATION_H
