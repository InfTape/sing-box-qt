#ifndef NODEEDITDIALOG_H
#define NODEEDITDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include "widgets/MenuComboBox.h"

class QVBoxLayout;
class NodeEditor;
class QTabWidget;
class QTextEdit;
class QScrollArea;
class QWidget;

class NodeEditDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NodeEditDialog(QWidget *parent = nullptr);
    ~NodeEditDialog();

    void setNodeData(const QJsonObject &node);
    QJsonObject nodeData() const;

private slots:
    void onTypeChanged(const QString &type);
    void updatePreview();

private:
    void setupUI();
    NodeEditor* createEditor(const QString &type);

    MenuComboBox *m_typeCombo;
    QVBoxLayout *m_editorContainer;
    NodeEditor *m_currentEditor;
    QTabWidget *m_tabs; // Tab 1: Editor, Tab 2: JSON Preview
    QTextEdit *m_jsonPreview;
    QScrollArea *m_scrollArea;
    QWidget *m_editorPage;
};

#endif // NODEEDITDIALOG_H
