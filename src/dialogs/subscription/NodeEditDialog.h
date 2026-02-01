#ifndef NODEEDITDIALOG_H
#define NODEEDITDIALOG_H

#include <QDialog>
#include <QJsonObject>

#include "widgets/common/MenuComboBox.h"

class QVBoxLayout;
class NodeEditor;
class QTabWidget;
class QTextEdit;
class QScrollArea;
class QWidget;
class QCheckBox;
class QLabel;
class QToolButton;
class ThemeService;
class NodeEditDialog : public QDialog {
  Q_OBJECT
 public:
  explicit NodeEditDialog(ThemeService* themeService,
                          QWidget*      parent = nullptr);
  ~NodeEditDialog();

  void        setNodeData(const QJsonObject& node);
  QJsonObject nodeData() const;
  void        setRuleSets(const QStringList& sets, bool enableShared = true);
  QStringList ruleSets() const;
  bool        sharedRulesEnabled() const;

 private slots:
  void onTypeChanged(const QString& type);
  void updatePreview();

 private:
  void        setupUI();
  NodeEditor* createEditor(const QString& type);

  MenuComboBox* m_typeCombo;
  QVBoxLayout*  m_editorContainer;
  NodeEditor*   m_currentEditor;
  QTabWidget*   m_tabs;  // Tab 1: Editor, Tab 2: JSON Preview
  QTextEdit*    m_jsonPreview;
  QScrollArea*  m_scrollArea;
  QWidget*      m_editorPage;
  QCheckBox*    m_sharedRulesCheck;
  QToolButton*  m_ruleSetsBtn;
  QStringList   m_ruleSets;
  ThemeService* m_themeService = nullptr;
};
#endif  // NODEEDITDIALOG_H
