#include "ProxyView.h"
#include <QItemSelection>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QStyledItemDelegate>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include "app/interfaces/ThemeService.h"
#include "core/DelayTestService.h"
#include "dialogs/subscription/NodeEditDialog.h"
#include "views/components/ProxyToolbar.h"
#include "views/components/ProxyTreePanel.h"
#include "views/proxy/ProxyTreePresenter.h"
#include "views/proxy/ProxyTreeUtils.h"
#include "views/proxy/ProxyViewController.h"
#include "widgets/common/RoundedMenu.h"

namespace {
class ProxyTreeDelegate : public QStyledItemDelegate {
 public:
  explicit ProxyTreeDelegate(ThemeService* themeService,
                             QObject*      parent = nullptr)
      : QStyledItemDelegate(parent), m_themeService(themeService) {}

  void initStyleOption(QStyleOptionViewItem* option,
                       const QModelIndex&    index) const override {
    QStyledItemDelegate::initStyleOption(option, index);
    if (!m_themeService) {
      return;
    }
    const QString state = index.data(Qt::UserRole + 2).toString();
    if (index.column() == 0 && state == "active") {
      option->palette.setColor(QPalette::Text,
                               m_themeService->color("success"));
    }
    if (index.column() == 2) {
      if (state == "loading") {
        option->palette.setColor(QPalette::Text,
                                 m_themeService->color("text-tertiary"));
      } else if (state == "ok") {
        option->palette.setColor(QPalette::Text,
                                 m_themeService->color("success"));
      } else if (state == "warn") {
        option->palette.setColor(QPalette::Text,
                                 m_themeService->color("warning"));
      } else if (state == "bad") {
        option->palette.setColor(QPalette::Text,
                                 m_themeService->color("error"));
      }
    }
  }

 private:
  ThemeService* m_themeService;
};
}  // namespace

ProxyView::ProxyView(ThemeService* themeService, QWidget* parent)
    : QWidget(parent),
      m_testSelectedBtn(nullptr),
      m_themeService(themeService) {
  setupUI();
  updateStyle();
  if (m_themeService) {
    connect(m_themeService,
            &ThemeService::themeChanged,
            this,
            &ProxyView::updateStyle);
  }
}

void ProxyView::setupUI() {
  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(24, 24, 24, 24);
  mainLayout->setSpacing(16);
  QHBoxLayout* headerLayout = new QHBoxLayout;
  QVBoxLayout* titleLayout  = new QVBoxLayout;
  titleLayout->setSpacing(4);
  QLabel* titleLabel = new QLabel(tr("Proxy"));
  titleLabel->setObjectName("PageTitle");
  QLabel* subtitleLabel =
      new QLabel(tr("Select proxy nodes and run latency tests"));
  subtitleLabel->setObjectName("PageSubtitle");
  titleLayout->addWidget(titleLabel);
  titleLayout->addWidget(subtitleLabel);
  headerLayout->addLayout(titleLayout);
  headerLayout->addStretch();
  mainLayout->addLayout(headerLayout);
  m_testSelectedBtn = nullptr;
  m_toolbar         = new ProxyToolbar(this);
  mainLayout->addWidget(m_toolbar);
  m_treePanel     = new ProxyTreePanel(this);
  m_treeWidget    = m_treePanel->treeWidget();
  m_treePresenter = std::make_unique<ProxyTreePresenter>(m_treeWidget);
  m_treeWidget->setItemDelegate(
      new ProxyTreeDelegate(m_themeService, m_treeWidget));
  connect(m_treeWidget->selectionModel(),
          &QItemSelectionModel::selectionChanged,
          this,
          &ProxyView::onSelectionChanged);
  mainLayout->addWidget(m_treePanel, 1);
  connect(m_toolbar,
          &ProxyToolbar::searchTextChanged,
          this,
          &ProxyView::onSearchTextChanged);
  connect(m_toolbar,
          &ProxyToolbar::testAllClicked,
          this,
          &ProxyView::onTestAllClicked);
  connect(m_toolbar, &ProxyToolbar::refreshClicked, this, &ProxyView::refresh);
  connect(
      m_treeWidget, &QTreeWidget::itemClicked, this, &ProxyView::onItemClicked);
  connect(m_treeWidget,
          &QTreeWidget::itemDoubleClicked,
          this,
          &ProxyView::onItemDoubleClicked);
  connect(m_treeWidget,
          &QTreeWidget::customContextMenuRequested,
          this,
          &ProxyView::onTreeContextMenu);
}

void ProxyView::updateStyle() {
  if (!m_themeService) {
    return;
  }
  setStyleSheet(m_themeService->loadStyleSheet(":/styles/proxy_view.qss"));
  ProxyTreeUtils::applyTreeItemColors(m_treeWidget, m_cachedProxies);
  if (m_treeWidget && m_treeWidget->viewport()) {
    m_treeWidget->viewport()->update();
  }
  updateTestButtonStyle(isTesting());
}

void ProxyView::setController(ProxyViewController* controller) {
  if (m_controller == controller) {
    return;
  }
  if (m_controller) {
    disconnect(m_controller, nullptr, this, nullptr);
  }
  m_controller = controller;
  if (!m_controller) {
    return;
  }
  connect(m_controller,
          &ProxyViewController::proxiesUpdated,
          this,
          &ProxyView::onProxiesReceived);
  connect(m_controller,
          &ProxyViewController::proxySelected,
          this,
          &ProxyView::onProxySelected);
  connect(m_controller,
          &ProxyViewController::proxySelectFailed,
          this,
          &ProxyView::onProxySelectFailed);
  connect(m_controller,
          &ProxyViewController::delayResult,
          this,
          &ProxyView::onDelayResult);
  connect(m_controller,
          &ProxyViewController::testProgress,
          this,
          &ProxyView::onTestProgress);
  connect(m_controller,
          &ProxyViewController::testCompleted,
          this,
          &ProxyView::onTestCompleted);
  connect(m_controller,
          &ProxyViewController::speedTestResult,
          this,
          &ProxyView::onSpeedTestResult);
}

void ProxyView::refresh() {
  if (m_controller) {
    m_controller->refreshProxies();
  }
}

void ProxyView::onProxiesReceived(const QJsonObject& proxies) {
  if (!m_treePresenter) {
    return;
  }
  m_cachedProxies = m_treePresenter->render(
      proxies,
      [this](int delay) { return formatDelay(delay); },
      [this](int count) { return tr("%1 nodes").arg(count); },
      [this](const QString& proxy) { return tr("Current: %1").arg(proxy); });
}

void ProxyView::onItemClicked(QTreeWidgetItem* item, int column) {
  Q_UNUSED(column)
  handleNodeActivation(item);
}

void ProxyView::onItemDoubleClicked(QTreeWidgetItem* item, int column) {
  Q_UNUSED(column)
  handleNodeActivation(item);
}

void ProxyView::onTreeContextMenu(const QPoint& pos) {
  QTreeWidgetItem* item = m_treeWidget->itemAt(pos);
  if (!item) {
    return;
  }
  const QString role = item->data(0, Qt::UserRole).toString();
  if (role != "node") {
    return;
  }
  RoundedMenu menu(this);
  menu.setObjectName("TrayMenu");
  if (m_themeService) {
    menu.setThemeColors(m_themeService->color("bg-secondary"),
                        m_themeService->color("primary"));
    connect(
        m_themeService, &ThemeService::themeChanged, &menu, [&menu, this]() {
          menu.setThemeColors(m_themeService->color("bg-secondary"),
                              m_themeService->color("primary"));
        });
  }
  QAction* detailAct = menu.addAction(tr("Details"));
  QAction* testAct   = menu.addAction(tr("Latency Test"));
  QAction* speedAct  = menu.addAction(tr("Speed Test"));
  QAction* chosen    = menu.exec(m_treeWidget->viewport()->mapToGlobal(pos));
  if (!chosen) {
    return;
  }
  if (chosen == detailAct) {
    QString nodeName = item->data(0, Qt::UserRole + 3).toString();
    if (nodeName.isEmpty()) {
      nodeName = item->text(0);
    }
    if (nodeName.startsWith("* ")) {
      nodeName = nodeName.mid(2);
    }
    QJsonObject nodeObj = loadNodeOutbound(nodeName);
    if (nodeObj.isEmpty()) {
      nodeObj = m_cachedProxies.value(nodeName).toObject();
    }
    if (nodeObj.isEmpty()) {
      QMessageBox::warning(
          this, tr("Node Details"), tr("Node data not found."));
      return;
    }
    NodeEditDialog dialog(m_themeService, this);
    dialog.setWindowTitle(tr("Node Details"));
    dialog.setNodeData(nodeObj);
    dialog.exec();
  } else if (chosen == testAct) {
    // trigger single node delay test
    QString nodeName = item->data(0, Qt::UserRole + 3).toString();
    if (nodeName.startsWith("* ")) {
      nodeName = nodeName.mid(2);
    }
    if (!nodeName.isEmpty() && m_controller) {
      m_testingNodes.clear();
      m_testingNodes.insert(nodeName);
      m_singleTesting       = true;
      m_singleTestingTarget = nodeName;
      m_controller->startSingleDelayTest(nodeName);
      updateTestButtonStyle(true);
    }
  } else if (chosen == speedAct) {
    startSpeedTest(item);
  }
}

QJsonObject ProxyView::loadNodeOutbound(const QString& tag) const {
  if (!m_controller) {
    return QJsonObject();
  }
  return m_controller->loadNodeOutbound(tag);
}

bool ProxyView::isTesting() const {
  return m_controller && m_controller->isTesting();
}

void ProxyView::handleNodeActivation(QTreeWidgetItem* item) {
  if (!item || !m_controller) {
    return;
  }
  const QString role = item->data(0, Qt::UserRole).toString();
  if (role != "node") {
    return;
  }
  QString group    = item->data(0, Qt::UserRole + 1).toString();
  QString nodeName = ProxyTreeUtils::nodeDisplayName(item);
  if (nodeName.startsWith("* ")) {
    nodeName = nodeName.mid(2);
  }
  m_pendingSelection.insert(group, nodeName);
  if (m_controller) {
    m_controller->selectProxy(group, nodeName);
  }
}

void ProxyView::onProxySelected(const QString& group, const QString& proxy) {
  if (!m_pendingSelection.contains(group) ||
      m_pendingSelection.value(group) != proxy) {
    return;
  }
  m_pendingSelection.remove(group);
  if (!m_treePresenter) {
    return;
  }
  m_treePresenter->updateSelectedProxy(
      m_cachedProxies, group, proxy, [this](const QString& now) {
        return tr("Current: %1").arg(now);
      });
}

void ProxyView::onProxySelectFailed(const QString& group,
                                    const QString& proxy) {
  if (m_pendingSelection.value(group) == proxy) {
    m_pendingSelection.remove(group);
  }
}

void ProxyView::startSpeedTest(QTreeWidgetItem* item) {
  if (!item) {
    return;
  }
  const QString nodeName  = item->data(0, Qt::UserRole + 3).toString();
  const QString groupName = item->data(0, Qt::UserRole + 1).toString();
  if (nodeName.isEmpty()) {
    return;
  }
  ProxyTreeUtils::updateNodeRowDelay(
      m_treeWidget, item, tr("Testing..."), "testing");
  if (m_controller) {
    m_controller->startSpeedTest(nodeName, groupName);
  }
}

void ProxyView::onSpeedTestResult(const QString& nodeName,
                                  const QString& result) {
  if (!m_treeWidget) {
    return;
  }
  QTreeWidgetItemIterator it(m_treeWidget);
  while (*it) {
    QTreeWidgetItem* item = *it;
    if (item->data(0, Qt::UserRole).toString() == "node") {
      QString name = ProxyTreeUtils::nodeDisplayName(item);
      if (name.startsWith("* ")) {
        name = name.mid(2);
      }
      if (name == nodeName) {
        const QString group = item->data(0, Qt::UserRole + 1).toString();
        const QString now =
            m_cachedProxies.value(group).toObject().value("now").toString();
        const QString delayText = result.isEmpty() ? tr("N/A") : result;
        ProxyTreeUtils::markNodeState(m_treeWidget, item, now, delayText);
        break;
      }
    }
    ++it;
  }
}

void ProxyView::onTestSelectedClicked() {
  if (!m_controller || !m_treeWidget) {
    return;
  }
  if (isTesting() && !m_testingNodes.isEmpty()) {
    return;
  }
  const auto selected = m_treeWidget->selectedItems();
  if (selected.isEmpty()) {
    return;
  }
  QTreeWidgetItem* item = selected.first();
  if (!item || item->data(0, Qt::UserRole).toString() != "node") {
    return;
  }
  QString name = ProxyTreeUtils::nodeDisplayName(item);
  if (name.startsWith("* ")) {
    name = name.mid(2);
  }
  if (name == "DIRECT" || name == "REJECT" || name == "COMPATIBLE") {
    return;
  }
  item->setText(2, "...");
  item->setData(2, Qt::UserRole + 2, QString("loading"));
  m_testingNodes.insert(name);
  m_singleTesting       = true;
  m_singleTestingTarget = name;
  if (m_testSelectedBtn) {
    m_testSelectedBtn->setEnabled(false);
    m_testSelectedBtn->setProperty("testing", true);
    m_testSelectedBtn->style()->unpolish(m_testSelectedBtn);
    m_testSelectedBtn->style()->polish(m_testSelectedBtn);
  }
  if (m_controller) {
    m_controller->startSingleDelayTest(name);
  }
}

void ProxyView::onTestAllClicked() {
  if (!m_controller) {
    return;
  }
  if (m_singleTesting) {
    return;
  }
  if (isTesting()) {
    if (m_controller) {
      m_controller->stopAllTests();
    }
    if (m_toolbar) {
      m_toolbar->setTestAllText(tr("Test All"));
    }
    updateTestButtonStyle(false);
    return;
  }
  QStringList             nodesToTest;
  QTreeWidgetItemIterator it(m_treeWidget);
  while (*it) {
    if ((*it)->data(0, Qt::UserRole).toString() == "node") {
      QString name = ProxyTreeUtils::nodeDisplayName(*it);
      if (name.startsWith("* ")) {
        name = name.mid(2);
      }
      if (name != "DIRECT" && name != "REJECT" && name != "COMPATIBLE") {
        nodesToTest.append(name);
      }
      (*it)->setText(2, "...");
      (*it)->setData(2, Qt::UserRole + 2, QString("loading"));
    }
    ++it;
  }
  if (nodesToTest.isEmpty()) {
    return;
  }
  nodesToTest.removeDuplicates();
  m_testingNodes.clear();
  for (const QString& node : nodesToTest) {
    m_testingNodes.insert(node);
  }
  if (m_toolbar) {
    m_toolbar->setTestAllText(tr("Stop Tests"));
  }
  updateTestButtonStyle(true);
  if (m_toolbar) {
    m_toolbar->showProgress(true);
    m_toolbar->setProgress(0);
  }
  if (m_controller) {
    m_controller->startBatchDelayTests(nodesToTest);
  }
}

void ProxyView::onSearchTextChanged(const QString& text) {
  ProxyTreeUtils::filterTreeNodes(m_treeWidget, text);
}

void ProxyView::onDelayResult(const ProxyDelayTestResult& result) {
  QString displayText;
  if (result.ok) {
    displayText = formatDelay(result.delay);
  } else {
    displayText = tr("Timeout");
  }
  QTreeWidgetItemIterator it(m_treeWidget);
  while (*it) {
    QString name = ProxyTreeUtils::nodeDisplayName(*it);
    if (name.startsWith("* ")) {
      name = name.mid(2);
    }
    if (name == result.proxy) {
      const QString group = (*it)->data(0, Qt::UserRole + 1).toString();
      const QString now =
          m_cachedProxies.value(group).toObject().value("now").toString();
      ProxyTreeUtils::markNodeState(m_treeWidget, *it, now, displayText);
    }
    ++it;
  }
  m_testingNodes.remove(result.proxy);
  if (m_singleTesting && result.proxy == m_singleTestingTarget) {
    m_singleTesting = false;
    m_singleTestingTarget.clear();
    if (m_testSelectedBtn) {
      m_testSelectedBtn->setEnabled(true);
      m_testSelectedBtn->setProperty("testing", false);
      m_testSelectedBtn->style()->unpolish(m_testSelectedBtn);
      m_testSelectedBtn->style()->polish(m_testSelectedBtn);
    }
  }
  if (QTreeWidgetItem* current = m_treeWidget->currentItem()) {
    ProxyTreeUtils::updateNodeRowSelected(
        m_treeWidget, current, current->isSelected());
  }
}

void ProxyView::onTestProgress(int current, int total) {
  if (total > 0) {
    int progress = (current * 100) / total;
    if (m_toolbar) {
      m_toolbar->setProgress(progress);
    }
  }
}

void ProxyView::onTestCompleted() {
  if (m_toolbar) {
    m_toolbar->setTestAllText(tr("Test All"));
  }
  updateTestButtonStyle(false);
  if (m_toolbar) {
    m_toolbar->showProgress(false);
  }
  m_testingNodes.clear();
  if (QTreeWidgetItem* current = m_treeWidget->currentItem()) {
    ProxyTreeUtils::updateNodeRowSelected(
        m_treeWidget, current, current->isSelected());
  }
}

QString ProxyView::formatDelay(int delay) const {
  if (delay <= 0) {
    return tr("Timeout");
  }
  return QString::number(delay) + " ms";
}

void ProxyView::updateTestButtonStyle(bool testing) {
  if (m_toolbar) {
    m_toolbar->setTesting(testing);
  }
  if (m_testSelectedBtn) {
    const bool busy = testing || m_singleTesting;
    m_testSelectedBtn->setEnabled(!busy);
    m_testSelectedBtn->setProperty("testing", busy);
    m_testSelectedBtn->style()->unpolish(m_testSelectedBtn);
    m_testSelectedBtn->style()->polish(m_testSelectedBtn);
  }
}

void ProxyView::onSelectionChanged(const QItemSelection& selected,
                                   const QItemSelection& deselected) {
  for (const QModelIndex& idx : selected.indexes()) {
    if (idx.column() != 0) {
      continue;
    }
    if (auto* item = m_treeWidget->itemFromIndex(idx)) {
      if (item->data(0, Qt::UserRole).toString() == "node") {
        ProxyTreeUtils::updateNodeRowSelected(m_treeWidget, item, true);
      }
    }
  }
  for (const QModelIndex& idx : deselected.indexes()) {
    if (idx.column() != 0) {
      continue;
    }
    if (auto* item = m_treeWidget->itemFromIndex(idx)) {
      if (item->data(0, Qt::UserRole).toString() == "node") {
        ProxyTreeUtils::updateNodeRowSelected(m_treeWidget, item, false);
      }
    }
  }
}
