#include "ProxyTreePanel.h"
#include <QFrame>
#include <QHeaderView>
#include <QTreeWidget>
#include <QVBoxLayout>

ProxyTreePanel::ProxyTreePanel(QWidget* parent) : QFrame(parent) {
  setObjectName("TreeCard");
  auto* rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(12, 12, 12, 12);
  rootLayout->setSpacing(0);
  m_treeWidget = new QTreeWidget(this);
  m_treeWidget->setObjectName("ProxyTree");
  m_treeWidget->setHeaderLabels({"", "", ""});
  m_treeWidget->setRootIsDecorated(false);
  m_treeWidget->setIndentation(0);
  m_treeWidget->setAlternatingRowColors(false);
  m_treeWidget->setHeaderHidden(true);
  m_treeWidget->header()->setDefaultAlignment(Qt::AlignCenter);
  m_treeWidget->setFrameShape(QFrame::NoFrame);
  m_treeWidget->header()->setStretchLastSection(false);
  m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_treeWidget->header()->setSectionResizeMode(1, QHeaderView::Fixed);
  m_treeWidget->header()->setSectionResizeMode(2, QHeaderView::Fixed);
  m_treeWidget->header()->resizeSection(1, 100);
  m_treeWidget->header()->resizeSection(2, 100);
  m_treeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
  m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  rootLayout->addWidget(m_treeWidget);
}
