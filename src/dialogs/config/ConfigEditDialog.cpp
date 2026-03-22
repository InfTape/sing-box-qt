#include "dialogs/config/ConfigEditDialog.h"

#include <QPushButton>
#include <QVBoxLayout>
#include "core/KernelService.h"
#include "widgets/config/JsonEditorWidget.h"

ConfigEditDialog::ConfigEditDialog(KernelService* kernelService,
                                   const QString& configPath,
                                   QWidget*       parent)
    : QDialog(parent), m_editor(new JsonEditorWidget(kernelService, this)) {
  setWindowTitle(tr("Edit current config"));
  setModal(true);
  setMinimumSize(820, 560);
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->addWidget(m_editor, 1);
  m_editor->setCheckConfigPath(configPath);
  QHBoxLayout* actions   = new QHBoxLayout;
  QPushButton* cancelBtn = new QPushButton(tr("Cancel"));
  QPushButton* okBtn     = new QPushButton(tr("Save and Apply"));
  cancelBtn->setAutoDefault(false);
  cancelBtn->setDefault(false);
  okBtn->setAutoDefault(false);
  okBtn->setDefault(false);
  actions->addStretch();
  actions->addWidget(cancelBtn);
  actions->addWidget(okBtn);
  layout->addLayout(actions);
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void ConfigEditDialog::setContent(const QString& content) {
  m_editor->setContent(content);
}

QString ConfigEditDialog::content() const {
  return m_editor->content();
}
