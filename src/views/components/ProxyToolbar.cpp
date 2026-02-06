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
  m_testAllBtn = new QPushButton(tr("Test All"), this);
  m_testAllBtn->setObjectName("TestAllBtn");
  m_testAllBtn->setCursor(Qt::PointingHandCursor);
  m_refreshBtn = new QPushButton(tr("Refresh"), this);
  m_refreshBtn->setObjectName("RefreshBtn");
  m_refreshBtn->setCursor(Qt::PointingHandCursor);
  toolbarLayout->addWidget(m_searchEdit, 1);
  toolbarLayout->addWidget(m_testAllBtn);
  toolbarLayout->addWidget(m_refreshBtn);
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
  connect(
      m_testAllBtn, &QPushButton::clicked, this, &ProxyToolbar::testAllClicked);
  connect(
      m_refreshBtn, &QPushButton::clicked, this, &ProxyToolbar::refreshClicked);
}

void ProxyToolbar::setTesting(bool testing) {
  if (!m_testAllBtn) {
    return;
  }
  m_testAllBtn->setProperty("testing", testing);
  m_testAllBtn->style()->unpolish(m_testAllBtn);
  m_testAllBtn->style()->polish(m_testAllBtn);
}

void ProxyToolbar::setTestAllText(const QString& text) {
  if (!m_testAllBtn) {
    return;
  }
  m_testAllBtn->setText(text);
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
