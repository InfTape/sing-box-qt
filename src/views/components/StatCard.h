#ifndef STATCARD_H
#define STATCARD_H

#include <QFrame>

class QLabel;
class ThemeService;

class StatCard : public QFrame {
  Q_OBJECT

 public:
  explicit StatCard(const QString& iconText, const QString& accentKey, const QString& title,
                    ThemeService* themeService = nullptr, QWidget* parent = nullptr);

  void setValueText(const QString& text);
  void setSubText(const QString& text);
  void updateStyle();

 private:
  void setupUi(const QString& title);
  void applyIcon();

 private:
  QString       m_iconText;
  QString       m_accentKey;
  QString       m_iconPath;
  ThemeService* m_themeService = nullptr;

  QLabel* m_iconLabel  = nullptr;
  QLabel* m_valueLabel = nullptr;
  QLabel* m_subLabel   = nullptr;
};

#endif  // STATCARD_H
