#ifndef JSONEDITORWIDGET_H
#define JSONEDITORWIDGET_H

#include <QTextDocument>
#include <QWidget>

class QEvent;
class QCheckBox;
class QLabel;
class QPushButton;
class QTimer;
class StyledLineEdit;
class JsonCodeEditor;
class KernelService;

class JsonEditorWidget : public QWidget {
  Q_OBJECT

 public:
  explicit JsonEditorWidget(KernelService* kernelService = nullptr,
                            QWidget*       parent        = nullptr);

  void    setContent(const QString& content);
  QString content() const;
  void    focusEditor();
  void    setCheckConfigPath(const QString& configPath);

 public slots:
  void showFindBar();
  void hideFindBar();
  void findNext();
  void findPrevious();
  void openGotoDialog();
  bool formatJson();
  bool runSingBoxCheck();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private slots:
  void handleFindTextChanged(const QString& text);
  void updateCursorLocation(int line, int column);
  void validateJsonDocument();

 private:
  bool                     performFind(bool backward);
  void                     setStatusMessage(const QString& text, bool isError = false);
  void                     focusFindInput();
  bool                     gotoLocationSpec(const QString& spec);
  QTextDocument::FindFlags currentFindFlags(bool backward) const;

  JsonCodeEditor* m_editor             = nullptr;
  QWidget*        m_findBar            = nullptr;
  StyledLineEdit* m_findEdit           = nullptr;
  QCheckBox*      m_caseSensitiveCheck = nullptr;
  QPushButton*    m_checkBtn           = nullptr;
  QLabel*         m_statusLabel        = nullptr;
  QLabel*         m_positionLabel      = nullptr;
  QTimer*         m_validationTimer    = nullptr;
  KernelService*  m_kernelService      = nullptr;
  QString         m_checkConfigPath;
  bool            m_hasValidationError = false;
};

#endif  // JSONEDITORWIDGET_H
