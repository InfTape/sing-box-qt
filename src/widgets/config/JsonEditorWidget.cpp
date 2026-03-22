#include "widgets/config/JsonEditorWidget.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QStringList>
#include <QTemporaryFile>
#include <QTimer>
#include <QVBoxLayout>
#include "core/KernelService.h"
#include "utils/AppPaths.h"
#include "widgets/common/StyledLineEdit.h"
#include "widgets/config/JsonCodeEditor.h"
#include "widgets/config/JsonSyntaxHighlighter.h"

namespace {

QPair<int, int> lineColumnFromUtf8Offset(const QByteArray& text, int offset) {
  const int safeOffset = qBound(0, offset, text.size());

  int line      = 1;
  int lineStart = 0;
  for (int i = 0; i < safeOffset; ++i) {
    if (text.at(i) == '\n') {
      ++line;
      lineStart = i + 1;
    }
  }

  const int column =
      QString::fromUtf8(text.constData() + lineStart, safeOffset - lineStart)
          .size() +
      1;

  return {line, column};
}

QString stripAnsiSequences(const QString& text) {
  QString sanitized = text;
  static const QRegularExpression ansiRegex(
      QStringLiteral(R"(\x1B\[[0-9;]*[A-Za-z])"));
  sanitized.remove(ansiRegex);
  return sanitized;
}

bool extractCheckErrorLocation(const QString& text, int* line, int* column) {
  const QString sanitized = stripAnsiSequences(text);
  static const QRegularExpression rowColumnRegex(
      QStringLiteral(R"(\brow\s+(\d+),\s*column\s+(\d+)\b)"),
      QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression lineColumnRegex(
      QStringLiteral(R"(\bline\s+(\d+),\s*column\s+(\d+)\b)"),
      QRegularExpression::CaseInsensitiveOption);

  for (const QRegularExpression& regex : {rowColumnRegex, lineColumnRegex}) {
    const QRegularExpressionMatch match = regex.match(sanitized);
    if (!match.hasMatch()) {
      continue;
    }

    bool lineOk   = false;
    bool columnOk = false;
    const int parsedLine = match.captured(1).toInt(&lineOk);
    const int parsedColumn = match.captured(2).toInt(&columnOk);
    if (!lineOk || !columnOk) {
      continue;
    }

    if (line) {
      *line = parsedLine;
    }
    if (column) {
      *column = parsedColumn;
    }
    return true;
  }

  return false;
}

}  // namespace

JsonEditorWidget::JsonEditorWidget(KernelService* kernelService, QWidget* parent)
    : QWidget(parent), m_kernelService(kernelService) {
  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(10);

  QHBoxLayout* toolbarLayout = new QHBoxLayout;
  toolbarLayout->setSpacing(8);

  QPushButton* findBtn = new QPushButton(tr("Find"), this);
  m_checkBtn           = new QPushButton(tr("Check Config"), this);
  QPushButton* gotoBtn = new QPushButton(tr("Go to"), this);
  QPushButton* formatBtn = new QPushButton(tr("Format JSON"), this);
  findBtn->setAutoDefault(false);
  m_checkBtn->setAutoDefault(false);
  gotoBtn->setAutoDefault(false);
  formatBtn->setAutoDefault(false);

  m_statusLabel = new QLabel(this);
  m_statusLabel->setMinimumWidth(180);

  m_positionLabel = new QLabel(this);
  m_positionLabel->setMinimumWidth(90);
  m_positionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  toolbarLayout->addWidget(findBtn);
  toolbarLayout->addWidget(m_checkBtn);
  toolbarLayout->addWidget(gotoBtn);
  toolbarLayout->addWidget(formatBtn);
  toolbarLayout->addStretch();
  toolbarLayout->addWidget(m_statusLabel);
  toolbarLayout->addWidget(m_positionLabel);
  mainLayout->addLayout(toolbarLayout);

  m_findBar = new QWidget(this);
  QHBoxLayout* findLayout = new QHBoxLayout(m_findBar);
  findLayout->setContentsMargins(0, 0, 0, 0);
  findLayout->setSpacing(8);

  QLabel* findLabel = new QLabel(tr("Find:"), m_findBar);
  m_findEdit        = new StyledLineEdit(m_findBar);
  m_findEdit->setPlaceholderText(tr("Press Enter or F3 to search"));
  m_caseSensitiveCheck = new QCheckBox(tr("Case sensitive"), m_findBar);
  QPushButton* previousBtn = new QPushButton(tr("Previous"), m_findBar);
  QPushButton* nextBtn     = new QPushButton(tr("Next"), m_findBar);
  QPushButton* closeBtn    = new QPushButton(tr("Close"), m_findBar);
  previousBtn->setAutoDefault(false);
  nextBtn->setAutoDefault(false);
  closeBtn->setAutoDefault(false);

  findLayout->addWidget(findLabel);
  findLayout->addWidget(m_findEdit, 1);
  findLayout->addWidget(m_caseSensitiveCheck);
  findLayout->addWidget(previousBtn);
  findLayout->addWidget(nextBtn);
  findLayout->addWidget(closeBtn);
  m_findBar->setVisible(false);
  mainLayout->addWidget(m_findBar);

  m_editor = new JsonCodeEditor(this);
  m_editor->setMinimumHeight(360);
  new JsonSyntaxHighlighter(m_editor->document());
  mainLayout->addWidget(m_editor, 1);

  m_validationTimer = new QTimer(this);
  m_validationTimer->setSingleShot(true);
  m_validationTimer->setInterval(260);

  connect(findBtn, &QPushButton::clicked, this, &JsonEditorWidget::showFindBar);
  connect(
      m_checkBtn, &QPushButton::clicked, this, &JsonEditorWidget::runSingBoxCheck);
  connect(gotoBtn, &QPushButton::clicked, this, &JsonEditorWidget::openGotoDialog);
  connect(formatBtn, &QPushButton::clicked, this, &JsonEditorWidget::formatJson);
  connect(previousBtn, &QPushButton::clicked, this, &JsonEditorWidget::findPrevious);
  connect(nextBtn, &QPushButton::clicked, this, &JsonEditorWidget::findNext);
  connect(closeBtn, &QPushButton::clicked, this, &JsonEditorWidget::hideFindBar);
  connect(m_findEdit,
          &QLineEdit::textChanged,
          this,
          &JsonEditorWidget::handleFindTextChanged);
  connect(m_findEdit, &QLineEdit::returnPressed, this, &JsonEditorWidget::findNext);
  connect(m_editor,
          &JsonCodeEditor::cursorLocationChanged,
          this,
          &JsonEditorWidget::updateCursorLocation);
  connect(m_editor, &QPlainTextEdit::textChanged, this, [this]() {
    m_validationTimer->start();
  });
  connect(m_validationTimer,
          &QTimer::timeout,
          this,
          &JsonEditorWidget::validateJsonDocument);

  m_findEdit->installEventFilter(this);

  auto* findShortcut = new QShortcut(QKeySequence::Find, this);
  connect(findShortcut, &QShortcut::activated, this, &JsonEditorWidget::showFindBar);

  auto* findNextShortcut = new QShortcut(QKeySequence::FindNext, this);
  connect(findNextShortcut, &QShortcut::activated, this, [this]() {
    if (!m_findBar->isVisible()) {
      showFindBar();
      return;
    }
    findNext();
  });

  auto* findPreviousShortcut = new QShortcut(QKeySequence::FindPrevious, this);
  connect(findPreviousShortcut, &QShortcut::activated, this, [this]() {
    if (!m_findBar->isVisible()) {
      showFindBar();
      return;
    }
    findPrevious();
  });

  auto* gotoShortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), this);
  connect(gotoShortcut, &QShortcut::activated, this, &JsonEditorWidget::openGotoDialog);

  updateCursorLocation(1, 1);
}

void JsonEditorWidget::setContent(const QString& content) {
  m_editor->setPlainText(content);
  m_editor->gotoLineColumn(1, 1);
  setStatusMessage(QString());
  validateJsonDocument();
}

QString JsonEditorWidget::content() const {
  return m_editor->toPlainText();
}

void JsonEditorWidget::focusEditor() {
  m_editor->setFocus();
}

void JsonEditorWidget::setCheckConfigPath(const QString& configPath) {
  m_checkConfigPath = configPath;
}

void JsonEditorWidget::showFindBar() {
  m_findBar->setVisible(true);

  QString selectedText = m_editor->textCursor().selectedText();
  if (!selectedText.contains(QChar(0x2029)) &&
      !selectedText.isEmpty() &&
      selectedText != m_findEdit->text()) {
    m_findEdit->setText(selectedText);
  }

  focusFindInput();
}

void JsonEditorWidget::hideFindBar() {
  m_findBar->setVisible(false);
  setStatusMessage(QString());
  focusEditor();
}

void JsonEditorWidget::findNext() {
  performFind(false);
}

void JsonEditorWidget::findPrevious() {
  performFind(true);
}

void JsonEditorWidget::openGotoDialog() {
  bool ok = false;
  const QString spec = QInputDialog::getText(this,
                                             tr("Go to line"),
                                             tr("Line or line:column"),
                                             QLineEdit::Normal,
                                             QString(),
                                             &ok);
  if (!ok) {
    return;
  }

  if (!gotoLocationSpec(spec)) {
    setStatusMessage(tr("Invalid location"), true);
  }
}

bool JsonEditorWidget::formatJson() {
  const QString currentText = content();
  if (currentText.trimmed().isEmpty()) {
    setStatusMessage(tr("Nothing to format"), true);
    return false;
  }

  const QByteArray utf8 = currentText.toUtf8();
  QJsonParseError error;
  const QJsonDocument doc = QJsonDocument::fromJson(utf8, &error);

  if (error.error != QJsonParseError::NoError || doc.isNull()) {
    const QPair<int, int> lineColumn =
        lineColumnFromUtf8Offset(utf8, static_cast<int>(error.offset));
    m_editor->setErrorMarker(lineColumn.first, lineColumn.second);
    m_hasValidationError = true;
    m_editor->gotoLineColumn(lineColumn.first, lineColumn.second);
    setStatusMessage(tr("Invalid JSON at %1:%2")
                         .arg(lineColumn.first)
                         .arg(lineColumn.second),
                     true);
    return false;
  }

  const QTextCursor cursor = m_editor->textCursor();
  const int         line   = cursor.blockNumber() + 1;
  const int         column = cursor.positionInBlock() + 1;

  m_editor->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
  m_editor->clearErrorMarker();
  m_hasValidationError = false;
  m_editor->gotoLineColumn(line, column);
  setStatusMessage(tr("JSON formatted"));
  return true;
}

bool JsonEditorWidget::runSingBoxCheck() {
  if (!m_kernelService) {
    QMessageBox::warning(this,
                         tr("Check Config"),
                         tr("Kernel service is unavailable"));
    return false;
  }

  QString workingDirectory = QFileInfo(m_checkConfigPath).absolutePath();
  if (workingDirectory.isEmpty()) {
    workingDirectory = QFileInfo(m_kernelService->getConfigPath()).absolutePath();
  }
  if (workingDirectory.isEmpty()) {
    workingDirectory = appDataDir();
  }
  if (!QDir().mkpath(workingDirectory)) {
    QMessageBox::warning(this,
                         tr("Check Config"),
                         tr("Failed to prepare working directory"));
    return false;
  }

  QTemporaryFile tempFile(
      QDir(workingDirectory).filePath("sing-box-check-XXXXXX.json"));
  tempFile.setAutoRemove(true);
  if (!tempFile.open()) {
    QMessageBox::warning(this,
                         tr("Check Config"),
                         tr("Failed to create temporary config file"));
    return false;
  }
  if (tempFile.write(content().toUtf8()) < 0) {
    QMessageBox::warning(this,
                         tr("Check Config"),
                         tr("Failed to write temporary config file"));
    return false;
  }
  tempFile.flush();
  tempFile.close();

  setStatusMessage(tr("Checking config..."));
  if (m_checkBtn) {
    m_checkBtn->setEnabled(false);
  }
  QApplication::setOverrideCursor(Qt::BusyCursor);
  QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  QString output;
  QString error;
  const bool ok = m_kernelService->checkConfigFile(
      tempFile.fileName(), workingDirectory, &output, &error);

  QApplication::restoreOverrideCursor();
  if (m_checkBtn) {
    m_checkBtn->setEnabled(true);
  }

  if (ok) {
    m_editor->clearErrorMarker();
    setStatusMessage(tr("sing-box check passed"));
    QMessageBox::information(
        this, tr("Check Config"), tr("sing-box check passed."));
    return true;
  }

  const QString detail = stripAnsiSequences(!output.isEmpty() ? output : error);
  const QString firstLine =
      detail.section(QLatin1Char('\n'), 0, 0).trimmed();
  int errorLine   = -1;
  int errorColumn = -1;
  if (extractCheckErrorLocation(detail, &errorLine, &errorColumn)) {
    m_editor->setErrorMarker(errorLine, errorColumn);
    m_editor->gotoLineColumn(errorLine, errorColumn);
    setStatusMessage(tr("sing-box check failed at %1:%2")
                         .arg(errorLine)
                         .arg(errorColumn),
                     true);
  } else {
    setStatusMessage(tr("sing-box check failed"), true);
  }

  QMessageBox box(this);
  box.setIcon(QMessageBox::Warning);
  box.setWindowTitle(tr("Check Config"));
  box.setText(tr("sing-box check failed."));
  if (!firstLine.isEmpty()) {
    box.setInformativeText(firstLine);
  }
  if (!detail.isEmpty()) {
    box.setDetailedText(detail);
  }
  box.exec();
  return false;
}

bool JsonEditorWidget::eventFilter(QObject* watched, QEvent* event) {
  if (watched == m_findEdit && event->type() == QEvent::KeyPress) {
    auto* keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->key() == Qt::Key_Escape) {
      hideFindBar();
      return true;
    }
    if (keyEvent->key() == Qt::Key_Return ||
        keyEvent->key() == Qt::Key_Enter) {
      if (keyEvent->modifiers() & Qt::ShiftModifier) {
        findPrevious();
      } else {
        findNext();
      }
      return true;
    }
  }

  return QWidget::eventFilter(watched, event);
}

void JsonEditorWidget::handleFindTextChanged(const QString& text) {
  if (text.isEmpty()) {
    setStatusMessage(QString());
    return;
  }

  setStatusMessage(tr("Press Enter or F3 to search"));
}

void JsonEditorWidget::updateCursorLocation(int line, int column) {
  m_positionLabel->setText(tr("Ln %1, Col %2").arg(line).arg(column));
}

void JsonEditorWidget::validateJsonDocument() {
  const QString currentText = content();
  if (currentText.trimmed().isEmpty()) {
    m_editor->clearErrorMarker();
    if (m_hasValidationError) {
      setStatusMessage(QString());
    }
    m_hasValidationError = false;
    return;
  }

  const QByteArray utf8 = currentText.toUtf8();
  QJsonParseError   error;
  const QJsonDocument doc = QJsonDocument::fromJson(utf8, &error);
  if (error.error == QJsonParseError::NoError && !doc.isNull()) {
    m_editor->clearErrorMarker();
    if (m_hasValidationError) {
      setStatusMessage(QString());
    }
    m_hasValidationError = false;
    return;
  }

  const QPair<int, int> lineColumn =
      lineColumnFromUtf8Offset(utf8, static_cast<int>(error.offset));
  m_editor->setErrorMarker(lineColumn.first, lineColumn.second);
  setStatusMessage(tr("Invalid JSON at %1:%2")
                       .arg(lineColumn.first)
                       .arg(lineColumn.second),
                   true);
  m_hasValidationError = true;
}

bool JsonEditorWidget::performFind(bool backward) {
  const QString text = m_findEdit->text();
  if (text.isEmpty()) {
    setStatusMessage(tr("Enter text to find"), true);
    focusFindInput();
    return false;
  }

  const bool found = m_editor->findText(text, currentFindFlags(backward));
  setStatusMessage(found ? tr("Match found") : tr("No matches found"), !found);
  if (!found) {
    focusFindInput();
  }
  return found;
}

void JsonEditorWidget::setStatusMessage(const QString& text, bool isError) {
  m_statusLabel->setText(text);
  m_statusLabel->setStyleSheet(isError ? "color: #d9534f;" : QString());
}

void JsonEditorWidget::focusFindInput() {
  m_findEdit->setFocus();
  m_findEdit->selectAll();
}

bool JsonEditorWidget::gotoLocationSpec(const QString& spec) {
  const QString trimmed = spec.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }

  const QStringList parts = trimmed.split(QLatin1Char(':'));
  bool              lineOk = false;
  bool              columnOk = true;
  const int         line = parts.value(0).toInt(&lineOk);
  int               column = 1;

  if (parts.size() > 2 || !lineOk) {
    return false;
  }

  if (parts.size() == 2) {
    column = parts.value(1).toInt(&columnOk);
    if (!columnOk) {
      return false;
    }
  }

  if (!m_editor->gotoLineColumn(line, column)) {
    return false;
  }

  setStatusMessage(tr("Moved to %1:%2").arg(line).arg(qMax(1, column)));
  return true;
}

QTextDocument::FindFlags JsonEditorWidget::currentFindFlags(bool backward) const {
  QTextDocument::FindFlags flags;
  if (backward) {
    flags |= QTextDocument::FindBackward;
  }
  if (m_caseSensitiveCheck->isChecked()) {
    flags |= QTextDocument::FindCaseSensitively;
  }
  return flags;
}
