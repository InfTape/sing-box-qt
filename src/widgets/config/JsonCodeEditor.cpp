#include "widgets/config/JsonCodeEditor.h"

#include <QFontDatabase>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTextBlock>
#include <QTextOption>

class JsonCodeEditorLineNumberArea : public QWidget {
 public:
  explicit JsonCodeEditorLineNumberArea(JsonCodeEditor* editor)
      : QWidget(editor), m_editor(editor) {}

  QSize sizeHint() const override {
    return QSize(m_editor->lineNumberAreaWidth(), 0);
  }

 protected:
  void paintEvent(QPaintEvent* event) override {
    m_editor->lineNumberAreaPaintEvent(event);
  }

 private:
  JsonCodeEditor* m_editor;
};

JsonCodeEditor::JsonCodeEditor(QWidget* parent)
    : StyledPlainTextEdit(parent),
      m_lineNumberArea(new JsonCodeEditorLineNumberArea(this)) {
  setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  setLineWrapMode(QPlainTextEdit::NoWrap);
  setWordWrapMode(QTextOption::NoWrap);
  setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);

  connect(this,
          &QPlainTextEdit::blockCountChanged,
          this,
          &JsonCodeEditor::updateLineNumberAreaWidth);
  connect(this,
          &QPlainTextEdit::updateRequest,
          this,
          &JsonCodeEditor::updateLineNumberArea);
  connect(this,
          &QPlainTextEdit::cursorPositionChanged,
          this,
          &JsonCodeEditor::highlightCurrentLine);
  connect(this,
          &QPlainTextEdit::cursorPositionChanged,
          this,
          &JsonCodeEditor::emitCursorLocation);

  updateLineNumberAreaWidth(0);
  updateExtraSelections();
  emitCursorLocation();
}

int JsonCodeEditor::lineNumberAreaWidth() const {
  int digits = 1;
  int maxVal = qMax(1, blockCount());
  while (maxVal >= 10) {
    maxVal /= 10;
    ++digits;
  }

  return 16 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

bool JsonCodeEditor::findText(const QString&                text,
                              QTextDocument::FindFlags flags) {
  if (text.isEmpty()) {
    return false;
  }

  QTextCursor searchCursor = textCursor();
  if (searchCursor.hasSelection()) {
    const int anchor = (flags & QTextDocument::FindBackward)
                           ? searchCursor.selectionStart()
                           : searchCursor.selectionEnd();
    searchCursor.setPosition(anchor);
  }

  QTextCursor match = document()->find(text, searchCursor, flags);
  if (match.isNull()) {
    QTextCursor wrapCursor(document());
    wrapCursor.movePosition((flags & QTextDocument::FindBackward)
                                ? QTextCursor::End
                                : QTextCursor::Start);
    match = document()->find(text, wrapCursor, flags);
  }

  if (match.isNull()) {
    return false;
  }

  setTextCursor(match);
  centerCursor();
  return true;
}

bool JsonCodeEditor::gotoLineColumn(int line, int column) {
  if (line < 1) {
    return false;
  }

  const QTextBlock block = document()->findBlockByLineNumber(line - 1);
  if (!block.isValid()) {
    return false;
  }

  const int safeColumn = qMax(1, column);
  const int offset     = qMin(safeColumn - 1, block.text().size());

  QTextCursor cursor = textCursor();
  cursor.setPosition(block.position() + offset);
  setTextCursor(cursor);
  centerCursor();
  setFocus();
  return true;
}

void JsonCodeEditor::setErrorMarker(int line, int column) {
  const int newLine   = qMax(1, line);
  const int newColumn = qMax(1, column);
  if (m_errorLine == newLine && m_errorColumn == newColumn) {
    return;
  }

  m_errorLine   = newLine;
  m_errorColumn = newColumn;
  updateExtraSelections();
  m_lineNumberArea->update();
}

void JsonCodeEditor::clearErrorMarker() {
  if (m_errorLine < 0 && m_errorColumn < 0) {
    return;
  }

  m_errorLine   = -1;
  m_errorColumn = -1;
  updateExtraSelections();
  m_lineNumberArea->update();
}

void JsonCodeEditor::resizeEvent(QResizeEvent* event) {
  StyledPlainTextEdit::resizeEvent(event);

  const QRect cr = contentsRect();
  m_lineNumberArea->setGeometry(
      QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void JsonCodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */) {
  setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void JsonCodeEditor::updateLineNumberArea(const QRect& rect, int dy) {
  if (dy != 0) {
    m_lineNumberArea->scroll(0, dy);
  } else {
    m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
  }

  if (rect.contains(viewport()->rect())) {
    updateLineNumberAreaWidth(0);
  }
}

void JsonCodeEditor::highlightCurrentLine() {
  updateExtraSelections();
}

void JsonCodeEditor::updateExtraSelections() {
  QList<QTextEdit::ExtraSelection> selections;

  QTextEdit::ExtraSelection selection;
  QColor                    lineColor = palette().alternateBase().color();
  lineColor.setAlpha(180);

  selection.format.setBackground(lineColor);
  selection.format.setProperty(QTextFormat::FullWidthSelection, true);
  selection.cursor = textCursor();
  selection.cursor.clearSelection();
  selections.append(selection);

  if (m_errorLine > 0) {
    const QTextBlock block = document()->findBlockByLineNumber(m_errorLine - 1);
    if (block.isValid()) {
      QTextEdit::ExtraSelection errorLineSelection;
      QColor                    errorLineColor(220, 38, 38, 32);
      errorLineSelection.format.setBackground(errorLineColor);
      errorLineSelection.format.setProperty(QTextFormat::FullWidthSelection,
                                            true);
      errorLineSelection.cursor = QTextCursor(block);
      errorLineSelection.cursor.clearSelection();
      selections.append(errorLineSelection);

      const int blockTextLength = block.text().size();
      if (blockTextLength > 0) {
        const int offsetInBlock = qBound(0, m_errorColumn - 1, blockTextLength);
        int       startPos      = block.position() + offsetInBlock;
        int       endPos        = block.position() + blockTextLength;
        if (startPos >= endPos) {
          startPos = endPos - 1;
        }

        QTextEdit::ExtraSelection underlineSelection;
        underlineSelection.format.setUnderlineColor(QColor("#DC2626"));
        underlineSelection.format.setUnderlineStyle(
            QTextCharFormat::WaveUnderline);
        underlineSelection.cursor = textCursor();
        underlineSelection.cursor.setPosition(startPos);
        underlineSelection.cursor.setPosition(endPos, QTextCursor::KeepAnchor);
        selections.append(underlineSelection);
      }
    }
  }

  setExtraSelections(selections);
}

void JsonCodeEditor::emitCursorLocation() {
  const QTextCursor cursor = textCursor();
  emit cursorLocationChanged(cursor.blockNumber() + 1,
                             cursor.positionInBlock() + 1);
}

void JsonCodeEditor::lineNumberAreaPaintEvent(QPaintEvent* event) {
  QPainter painter(m_lineNumberArea);
  painter.fillRect(event->rect(), palette().alternateBase());

  QTextBlock block       = firstVisibleBlock();
  int        blockNumber = block.blockNumber();
  int        top =
      qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + qRound(blockBoundingRect(block).height());

  const int currentBlockNumber = textCursor().blockNumber();

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      const QString lineNumber = QString::number(blockNumber + 1);
      if (blockNumber + 1 == m_errorLine) {
        painter.setPen(QColor("#DC2626"));
      } else {
        painter.setPen(blockNumber == currentBlockNumber
                           ? palette().color(QPalette::Highlight)
                           : palette().color(QPalette::Mid));
      }
      painter.drawText(0,
                       top,
                       m_lineNumberArea->width() - 8,
                       fontMetrics().height(),
                       Qt::AlignRight | Qt::AlignVCenter,
                       lineNumber);
    }

    block = block.next();
    top   = bottom;
    bottom += qRound(blockBoundingRect(block).height());
    ++blockNumber;
  }
}
