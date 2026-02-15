#include "ProxyToolbar.h"
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

ProxyToolbar::ProxyToolbar(QWidget* parent) : QFrame(parent) {
  setObjectName("ToolbarCard");
  auto* rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(14, 12, 14, 12);
  rootLayout->setSpacing(12);
  auto* toolbarLayout = new QHBoxLayout;
  toolbarLayout->setContentsMargins(0, 0, 0, 0);
  toolbarLayout->setSpacing(12);
  m_searchEdit = new QLineEdit(this);
  m_searchEdit->setPlaceholderText(tr("Search nodes..."));
  m_searchEdit->setObjectName("SearchInput");
  m_searchEdit->setClearButtonEnabled(true);
  m_addGroupBtn = new QPushButton(tr("Add Group"), this);
  m_addGroupBtn->setObjectName("AddGroupBtn");
  m_addGroupBtn->setCursor(Qt::PointingHandCursor);
  toolbarLayout->addWidget(m_searchEdit, 1);
  toolbarLayout->addWidget(m_addGroupBtn);
  m_progressBar = new QProgressBar(this);
  m_progressBar->setObjectName("ProxyProgress");
  m_progressBar->setRange(0, 100);
  m_progressBar->setValue(0);
  m_progressBar->setTextVisible(false);
  m_progressBar->setFixedHeight(4);
  m_progressBar->hide();
  rootLayout->addLayout(toolbarLayout);
  rootLayout->addWidget(m_progressBar);
  connect(m_searchEdit,
          &QLineEdit::textChanged,
          this,
          &ProxyToolbar::searchTextChanged);
  connect(m_addGroupBtn,
          &QPushButton::clicked,
          this,
          &ProxyToolbar::addGroupClicked);
}



void ProxyToolbar::setProgress(int progress) {
  if (!m_progressBar) {
    return;
  }
  m_progressBar->setValue(progress);
}

void ProxyToolbar::showProgress(bool visible) {
  if (!m_progressBar) {
    return;
  }
  m_progressBar->setVisible(visible);
}
