#ifndef CONFIGEDITDIALOG_H
#define CONFIGEDITDIALOG_H
#include <QDialog>
#include <QString>
class StyledTextEdit;

class ConfigEditDialog : public QDialog {
  Q_OBJECT
 public:
  explicit ConfigEditDialog(QWidget* parent = nullptr);
  void    setContent(const QString& content);
  QString content() const;

 private:
  StyledTextEdit* m_editor;
};
#endif  // CONFIGEDITDIALOG_H
