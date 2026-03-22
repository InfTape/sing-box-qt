#include "widgets/config/JsonSyntaxHighlighter.h"

#include <QApplication>
#include <QPalette>
#include <QRegularExpression>
#include <QTextDocument>
#include <QVector>

namespace {

struct Span {
  int start  = 0;
  int length = 0;
};

QTextCharFormat makeFormat(const QColor& color, int weight = QFont::Normal) {
  QTextCharFormat format;
  format.setForeground(color);
  format.setFontWeight(weight);
  return format;
}

bool isDarkMode() {
  return QApplication::palette().color(QPalette::Base).lightness() < 128;
}

bool overlapsStringSpan(const QVector<Span>& spans, int start, int length) {
  const int end = start + length;
  for (const Span& span : spans) {
    const int spanEnd = span.start + span.length;
    if (start < spanEnd && end > span.start) {
      return true;
    }
  }
  return false;
}

}  // namespace

JsonSyntaxHighlighter::JsonSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent) {
  initFormats();
}

void JsonSyntaxHighlighter::highlightBlock(const QString& text) {
  QVector<Span> stringSpans;

  for (int i = 0; i < text.size(); ++i) {
    if (text.at(i) != QLatin1Char('"')) {
      continue;
    }

    int  endIndex = i + 1;
    bool escaped  = false;
    while (endIndex < text.size()) {
      const QChar ch = text.at(endIndex);
      if (escaped) {
        escaped = false;
      } else if (ch == QLatin1Char('\\')) {
        escaped = true;
      } else if (ch == QLatin1Char('"')) {
        ++endIndex;
        break;
      }
      ++endIndex;
    }

    const int length = qMax(1, endIndex - i);
    stringSpans.append({i, length});

    int next = i + length;
    while (next < text.size() && text.at(next).isSpace()) {
      ++next;
    }
    const bool isKey =
        next < text.size() && text.at(next) == QLatin1Char(':');
    setFormat(i, length, isKey ? m_keyFormat : m_stringFormat);

    i = i + length - 1;
  }

  const auto applyRegexFormat = [this, &text, &stringSpans](
                                    const QRegularExpression& regex,
                                    const QTextCharFormat&    format) {
    auto it = regex.globalMatch(text);
    while (it.hasNext()) {
      const QRegularExpressionMatch match = it.next();
      if (overlapsStringSpan(
              stringSpans, match.capturedStart(), match.capturedLength())) {
        continue;
      }
      setFormat(match.capturedStart(), match.capturedLength(), format);
    }
  };

  static const QRegularExpression numberRegex(
      R"((?<![\w.])-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+\-]?\d+)?(?![\w.]))");
  static const QRegularExpression booleanRegex(R"(\b(?:true|false)\b)");
  static const QRegularExpression nullRegex(R"(\bnull\b)");
  static const QRegularExpression punctuationRegex(R"([{}\[\],:])");

  applyRegexFormat(numberRegex, m_numberFormat);
  applyRegexFormat(booleanRegex, m_booleanFormat);
  applyRegexFormat(nullRegex, m_nullFormat);
  applyRegexFormat(punctuationRegex, m_punctuationFormat);
}

void JsonSyntaxHighlighter::initFormats() {
  if (isDarkMode()) {
    m_keyFormat         = makeFormat(QColor("#9CDCFE"));
    m_stringFormat      = makeFormat(QColor("#CE9178"));
    m_numberFormat      = makeFormat(QColor("#B5CEA8"));
    m_booleanFormat     = makeFormat(QColor("#4FC1FF"), QFont::Bold);
    m_nullFormat        = makeFormat(QColor("#C586C0"), QFont::Bold);
    m_punctuationFormat = makeFormat(QColor("#D4D4D4"));
    return;
  }

  m_keyFormat         = makeFormat(QColor("#7C3AED"));
  m_stringFormat      = makeFormat(QColor("#9A3412"));
  m_numberFormat      = makeFormat(QColor("#1D4ED8"));
  m_booleanFormat     = makeFormat(QColor("#059669"), QFont::Bold);
  m_nullFormat        = makeFormat(QColor("#DC2626"), QFont::Bold);
  m_punctuationFormat = makeFormat(QColor("#6B7280"));
}
