#ifndef CONFIGEDITDIALOG_H
#define CONFIGEDITDIALOG_H
#include <QDialog>
#include <QString>
class JsonEditorWidget;
class KernelService;

class ConfigEditDialog : public QDialog {
  Q_OBJECT
 public:
  explicit ConfigEditDialog(KernelService*  kernelService,
                            const QString&  configPath = QString(),
                            QWidget*        parent     = nullptr);
  void    setContent(const QString& content);
  QString content() const;

 private:
  JsonEditorWidget* m_editor = nullptr;
};
#endif  // CONFIGEDITDIALOG_H
