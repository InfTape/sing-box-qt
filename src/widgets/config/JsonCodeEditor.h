#ifndef JSONCODEEDITOR_H
#define JSONCODEEDITOR_H

#include <QTextDocument>
#include "widgets/common/StyledPlainTextEdit.h"

class QPaintEvent;
class QResizeEvent;
class JsonCodeEditorLineNumberArea;

class JsonCodeEditor : public StyledPlainTextEdit {
  Q_OBJECT

 public:
  explicit JsonCodeEditor(QWidget* parent = nullptr);

  int  lineNumberAreaWidth() const;
  bool findText(const QString& text, QTextDocument::FindFlags flags = {});
  bool gotoLineColumn(int line, int column = 1);
  void setErrorMarker(int line, int column);
  void clearErrorMarker();

 signals:
  void cursorLocationChanged(int line, int column);

 protected:
  void resizeEvent(QResizeEvent* event) override;

 private slots:
  void updateLineNumberAreaWidth(int newBlockCount);
  void updateLineNumberArea(const QRect& rect, int dy);
  void highlightCurrentLine();
  void emitCursorLocation();

 private:
  friend class JsonCodeEditorLineNumberArea;

  void updateExtraSelections();
  void lineNumberAreaPaintEvent(QPaintEvent* event);

  QWidget* m_lineNumberArea = nullptr;
  int      m_errorLine      = -1;
  int      m_errorColumn    = -1;
};

#endif  // JSONCODEEDITOR_H
