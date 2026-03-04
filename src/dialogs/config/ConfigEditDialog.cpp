#include "dialogs/config/ConfigEditDialog.h"
#include <QPushButton>
#include "widgets/common/StyledTextEdit.h"
#include <QVBoxLayout>

ConfigEditDialog::ConfigEditDialog(QWidget* parent)
    : QDialog(parent), m_editor(new StyledTextEdit) {
  setWindowTitle(tr("Edit current config"));
  setModal(true);
  setMinimumWidth(720);
  QVBoxLayout* layout = new QVBoxLayout(this);
  m_editor->setMinimumHeight(300);
  layout->addWidget(m_editor);
  QHBoxLayout* actions   = new QHBoxLayout;
  QPushButton* cancelBtn = new QPushButton(tr("Cancel"));
  QPushButton* okBtn     = new QPushButton(tr("Save and Apply"));
  actions->addStretch();
  actions->addWidget(cancelBtn);
  actions->addWidget(okBtn);
  layout->addLayout(actions);
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void ConfigEditDialog::setContent(const QString& content) {
  m_editor->setPlainText(content);
}

QString ConfigEditDialog::content() const {
  return m_editor->toPlainText();
}
