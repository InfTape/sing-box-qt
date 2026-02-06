#ifndef SUBSCRIPTIONFORMDIALOG_H
#define SUBSCRIPTIONFORMDIALOG_H
#include <QDialog>
#include <QString>
#include <QStringList>
#include "widgets/common/MenuComboBox.h"
class QCheckBox;
class QLabel;
class QTabWidget;
class QTextEdit;
class QToolButton;
class RoundedMenu;
class QLineEdit;
class ThemeService;

// Multi-select rule set dropdown widget
class MultiSelectMenuBox : public QWidget {
  Q_OBJECT
 public:
  explicit MultiSelectMenuBox(QWidget* parent = nullptr, ThemeService* themeService = nullptr);
  void        setOptions(const QStringList& options);
  void        setSelected(const QStringList& selected);
  QStringList selected() const;
 signals:
  void selectionChanged(const QStringList& list);

 private:
  void          rebuildMenu();
  void          updateButtonText();
  QToolButton*  m_button = nullptr;
  RoundedMenu*  m_menu   = nullptr;
  QStringList   m_options;
  QStringList   m_selected;
  ThemeService* m_themeService = nullptr;
};
struct SubscriptionInfo;

class SubscriptionFormDialog : public QDialog {
  Q_OBJECT
 public:
  explicit SubscriptionFormDialog(ThemeService* themeService, QWidget* parent = nullptr);
  void        setEditData(const SubscriptionInfo& info);
  QString     name() const;
  QString     url() const;
  QString     manualContent() const;
  QString     uriContent() const;
  bool        isManual() const;
  bool        isUriList() const;
  bool        useOriginalConfig() const;
  bool        sharedRulesEnabled() const;
  QStringList ruleSets() const;
  int         autoUpdateIntervalMinutes() const;
  bool        validateInput(QString* error) const;

 private:
  QWidget*            createTextTab(QTextEdit* edit, const QString& placeholder);
  int                 indexForInterval(int minutes) const;
  void                updateState();
  QLineEdit*          m_nameEdit;
  QTabWidget*         m_tabs;
  QTextEdit*          m_urlEdit;
  QTextEdit*          m_manualEdit;
  QTextEdit*          m_uriEdit;
  QCheckBox*          m_useOriginalCheck;
  QCheckBox*          m_sharedRulesCheck;
  MultiSelectMenuBox* m_ruleSetsBox;
  MenuComboBox*       m_autoUpdateCombo;
  QLabel*             m_hintLabel;
  ThemeService*       m_themeService = nullptr;
};
#endif  // SUBSCRIPTIONFORMDIALOG_H
