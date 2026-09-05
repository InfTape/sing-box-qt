#include "LogTextSelection.h"
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <algorithm>
#include "widgets/common/RoundedMenu.h"
#include "widgets/logs/LogRowWidget.h"

LogTextSelection::LogTextSelection(QScrollArea* area)
    : QObject(area), m_area(area), m_dragScroll(new QTimer(this)) {
  area->widget()->setFocusPolicy(Qt::StrongFocus);
  area->installEventFilter(this);
  area->widget()->installEventFilter(this);
  m_dragScroll->setInterval(40);
  connect(m_dragScroll, &QTimer::timeout, this, [this]() {
    const int y = m_area->viewport()->mapFromGlobal(m_mouseGlobal).y();
    const int height = m_area->viewport()->height();
    const int delta = y < 16 ? -20 : (y > height - 16 ? 20 : 0);
    if (delta) {
      auto* bar = m_area->verticalScrollBar();
      bar->setSliderPosition(bar->value() + delta);
      m_cursor = positionAt(m_mouseGlobal);
      updateSelection();
    }
  });
}

void LogTextSelection::watchRow(QWidget* row) {
  row->installEventFilter(this);
  for (auto* label : row->findChildren<QLabel*>()) {
    label->installEventFilter(this);
    if (label->objectName() != "LogContent") {
      continue;
    }
    label->setTextFormat(Qt::PlainText);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setCursor(Qt::IBeamCursor);
    connect(label, &QObject::destroyed, this, [this]() {
      m_labels.removeIf([](const auto& label) { return label.isNull(); });
    });
  }
}

void LogTextSelection::setRows(const QVector<LogRowWidget*>& rows) {
  clear();
  m_labels.clear();
  for (auto* row : rows) {
    m_labels.append(row->findChild<QLabel*>("LogContent"));
  }
}

bool LogTextSelection::isActive() const {
  return m_dragging || (m_anchor.row >= 0 &&
      (m_anchor.row != m_cursor.row || m_anchor.offset != m_cursor.offset));
}

void LogTextSelection::clear() {
  if (m_anchor.row < 0 && !m_dragging) {
    return;
  }
  m_dragging = false;
  m_dragScroll->stop();
  m_anchor = {};
  m_cursor = {};
  updateSelection();
}

LogTextSelection::Position LogTextSelection::positionAt(
    const QPoint& global) const {
  for (int i = 0; i < m_labels.size(); ++i) {
    const auto* label = m_labels[i].data();
    if (!label) {
      continue;
    }
    const QPoint local = label->mapFromGlobal(global);
    const QRect rect = label->contentsRect().adjusted(
        label->margin(), label->margin(), -label->margin(), -label->margin());
    if (local.y() < rect.top()) {
      return {i, 0};
    }
    if (local.y() > rect.bottom()) {
      continue;
    }
    // QLabel's selectable plain text uses a zero-margin QTextDocument too.
    QTextDocument document;
    document.setDocumentMargin(0);
    document.setDefaultFont(label->font());
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    option.setAlignment(label->alignment() & Qt::AlignHorizontal_Mask);
    document.setDefaultTextOption(option);
    document.setPlainText(label->text());
    document.setTextWidth(rect.width());
    const int offset = document.documentLayout()->hitTest(
        local - rect.topLeft(), Qt::FuzzyHit);
    return {i, qBound(0, offset, static_cast<int>(label->text().size()))};
  }
  if (m_labels.isEmpty()) {
    return {};
  }
  return {static_cast<int>(m_labels.size() - 1),
          static_cast<int>(m_labels.last()->text().size())};
}

void LogTextSelection::updateSelection() {
  Position start = m_anchor;
  Position end = m_cursor;
  if (start.row > end.row ||
      (start.row == end.row && start.offset > end.offset)) {
    std::swap(start, end);
  }
  for (int i = 0; i < m_labels.size(); ++i) {
    auto* label = m_labels[i].data();
    if (!label) {
      continue;
    }
    int from = 0;
    int to = 0;
    if (start.row >= 0 && i >= start.row && i <= end.row) {
      from = i == start.row ? start.offset : 0;
      to = i == end.row ? end.offset : static_cast<int>(label->text().size());
    }
    label->setSelection(from, to - from);
  }
  emit changed();
}

QString LogTextSelection::selectedText() const {
  Position start = m_anchor;
  Position end = m_cursor;
  if (start.row < 0 || end.row < 0) {
    return {};
  }
  if (start.row > end.row ||
      (start.row == end.row && start.offset > end.offset)) {
    std::swap(start, end);
  }
  QStringList lines;
  for (int i = start.row; i <= end.row && i < m_labels.size(); ++i) {
    if (!m_labels[i]) {
      continue;
    }
    const QString text = m_labels[i]->text();
    const int from = i == start.row ? start.offset : 0;
    const int to = i == end.row ? end.offset : static_cast<int>(text.size());
    lines.append(text.mid(from, to - from));
  }
  return lines.join('\n');
}

void LogTextSelection::copy() const {
  const QString text = selectedText();
  if (!text.isEmpty()) {
    QApplication::clipboard()->setText(text);
  }
}

void LogTextSelection::selectAll() {
  if (m_labels.isEmpty()) {
    return;
  }
  m_anchor = {0, 0};
  m_cursor = {static_cast<int>(m_labels.size() - 1),
              static_cast<int>(m_labels.last()->text().size())};
  updateSelection();
}

bool LogTextSelection::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::Hide ||
      event->type() == QEvent::WindowDeactivate ||
      event->type() == QEvent::UngrabMouse) {
    if (m_dragging) {
      m_dragging = false;
      m_dragScroll->stop();
      emit changed();
    }
  }
  if (event->type() == QEvent::KeyPress) {
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->matches(QKeySequence::Copy)) {
      copy();
      return true;
    }
    if (key->matches(QKeySequence::SelectAll)) {
      selectAll();
      return true;
    }
    if (key->key() == Qt::Key_Escape) {
      clear();
      return true;
    }
  }
  if (event->type() == QEvent::ContextMenu) {
    auto* context = static_cast<QContextMenuEvent*>(event);
    RoundedMenu menu(m_area);
    menu.setObjectName("ComboMenu");
    auto* copyAction = menu.addAction(QIcon::fromTheme("edit-copy"), tr("&Copy"));
    copyAction->setObjectName("edit-copy");
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setShortcutVisibleInContextMenu(true);
    copyAction->setEnabled(!selectedText().isEmpty());
    connect(copyAction, &QAction::triggered, this, &LogTextSelection::copy);
    menu.exec(context->globalPos());
    return true;
  }
  if (event->type() == QEvent::MouseButtonPress ||
      event->type() == QEvent::MouseButtonDblClick) {
    auto* mouse = static_cast<QMouseEvent*>(event);
    if (mouse->button() != Qt::LeftButton) {
      return false;
    }
    auto* label = qobject_cast<QLabel*>(watched);
    if (!label || label->objectName() != "LogContent") {
      clear();
      return false;
    }
    m_area->widget()->setFocus(Qt::MouseFocusReason);
    m_mouseGlobal = mouse->globalPosition().toPoint();
    m_cursor = positionAt(m_mouseGlobal);
    if (!(mouse->modifiers() & Qt::ShiftModifier) || m_anchor.row < 0) {
      m_anchor = m_cursor;
    }
    if (event->type() == QEvent::MouseButtonDblClick) {
      QTextDocument document(label->text());
      QTextCursor cursor(&document);
      cursor.setPosition(m_cursor.offset);
      cursor.select(QTextCursor::WordUnderCursor);
      m_anchor.row = m_cursor.row;
      m_anchor.offset = cursor.selectionStart();
      m_cursor.offset = cursor.selectionEnd();
    }
    m_dragging = true;
    m_dragScroll->start();
    updateSelection();
    return true;
  }
  if (event->type() == QEvent::MouseMove && m_dragging) {
    auto* mouse = static_cast<QMouseEvent*>(event);
    m_mouseGlobal = mouse->globalPosition().toPoint();
    m_cursor = positionAt(m_mouseGlobal);
    updateSelection();
    return true;
  }
  if (event->type() == QEvent::MouseButtonRelease && m_dragging) {
    auto* mouse = static_cast<QMouseEvent*>(event);
    if (mouse->button() != Qt::LeftButton) {
      return false;
    }
    m_dragging = false;
    m_dragScroll->stop();
    emit changed();
    return true;
  }
  return QObject::eventFilter(watched, event);
}
