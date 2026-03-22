#ifndef JSONSYNTAXHIGHLIGHTER_H
#define JSONSYNTAXHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class QTextDocument;

class JsonSyntaxHighlighter : public QSyntaxHighlighter {
  Q_OBJECT

 public:
  explicit JsonSyntaxHighlighter(QTextDocument* parent = nullptr);

 protected:
  void highlightBlock(const QString& text) override;

 private:
  void initFormats();

  QTextCharFormat m_keyFormat;
  QTextCharFormat m_stringFormat;
  QTextCharFormat m_numberFormat;
  QTextCharFormat m_booleanFormat;
  QTextCharFormat m_nullFormat;
  QTextCharFormat m_punctuationFormat;
};

#endif  // JSONSYNTAXHIGHLIGHTER_H
