#include "dialogs/rules/ManageRuleSetsDialog.h"
#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include "app/interfaces/ThemeService.h"
#include "dialogs/rules/RuleEditorDialog.h"
#include "services/rules/RuleConfigService.h"
#include "services/rules/SharedRulesStore.h"
#include "widgets/common/RoundedMenu.h"

ManageRuleSetsDialog::ManageRuleSetsDialog(ConfigRepository* configRepo,
                                           ThemeService*     themeService,
                                           QWidget*          parent)
    : QDialog(parent),
      m_list(new QListWidget(this)),
      m_ruleList(new QListWidget(this)),
      m_configRepo(configRepo),
      m_themeService(themeService) {
  setWindowTitle(tr("Manage Rule Sets"));
  setModal(true);
  resize(520, 380);
  QVBoxLayout* layout     = new QVBoxLayout(this);
  QHBoxLayout* listLayout = new QHBoxLayout;
  QVBoxLayout* setLayout  = new QVBoxLayout;
  setLayout->addWidget(new QLabel(tr("Rule Sets")));
  m_list->setSelectionMode(QAbstractItemView::SingleSelection);
  setLayout->addWidget(m_list, 1);
  QVBoxLayout* ruleLayout = new QVBoxLayout;
  ruleLayout->addWidget(new QLabel(tr("Rules in set")));
  m_ruleList->setSelectionMode(QAbstractItemView::SingleSelection);
  ruleLayout->addWidget(m_ruleList, 1);
  listLayout->addLayout(setLayout, 1);
  listLayout->addLayout(ruleLayout, 2);
  layout->addLayout(listLayout);
  // Context menus
  m_list->setContextMenuPolicy(Qt::CustomContextMenu);
  m_ruleList->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_list,
          &QListWidget::customContextMenuRequested,
          this,
          &ManageRuleSetsDialog::onSetContextMenu);
  connect(m_ruleList,
          &QListWidget::customContextMenuRequested,
          this,
          &ManageRuleSetsDialog::onRuleContextMenu);
  connect(m_list,
          &QListWidget::currentRowChanged,
          this,
          &ManageRuleSetsDialog::onSetChanged);
  reload();
}

void ManageRuleSetsDialog::reload() {
  m_list->clear();
  QStringList sets = SharedRulesStore::listRuleSets();
  sets.removeDuplicates();
  sets.sort();
  for (const auto& s : sets) {
    m_list->addItem(s);
  }
  if (m_list->count() > 0) {
    m_list->setCurrentRow(0);
  }
  reloadRules();
}

QString ManageRuleSetsDialog::selectedName() const {
  if (!m_list->currentItem()) {
    return QString();
  }
  return m_list->currentItem()->text().trimmed();
}

void ManageRuleSetsDialog::reloadRules() {
  m_ruleList->clear();
  const QString name = selectedName();
  if (name.isEmpty()) {
    return;
  }
  const QJsonArray rules = SharedRulesStore::loadRules(name);
  for (const auto& v : rules) {
    if (!v.isObject()) {
      continue;
    }
    const QJsonObject obj = v.toObject();
    QString           key;
    QString           payload;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      if (it.key() == "outbound" || it.key() == "action") {
        continue;
      }
      key = it.key();
      if (it->isArray()) {
        QStringList arr;
        for (const auto& av : it->toArray()) {
          arr << (av.isDouble() ? QString::number(av.toInt()) : av.toString());
        }
        payload = arr.join(",");
      } else if (it->isDouble()) {
        payload = QString::number(it->toInt());
      } else if (it->isBool()) {
        payload = it->toBool() ? "true" : "false";
      } else {
        payload = it->toString();
      }
      break;
    }
    const QString    outbound = obj.value("outbound").toString();
    const QString    text = QString("%1=%2  -> %3").arg(key, payload, outbound);
    QListWidgetItem* item = new QListWidgetItem(text, m_ruleList);
    item->setData(Qt::UserRole, obj);
  }
}

bool ManageRuleSetsDialog::confirmDelete(const QString& name) {
  return QMessageBox::question(this,
                               tr("Delete Rule Set"),
                               tr("Delete rule set '%1'?").arg(name),
                               QMessageBox::Yes | QMessageBox::No,
                               QMessageBox::No) == QMessageBox::Yes;
}

void ManageRuleSetsDialog::addRuleToSet(
    const QString& setName, const RuleConfigService::RuleEditData& dataIn) {
  QString                         error;
  RuleConfigService::RuleEditData normalized = dataIn;
  normalized.ruleSet                         = setName;
  QJsonObject obj;
  if (!RuleConfigService::buildRouteRulePublic(normalized, &obj, &error)) {
    QMessageBox::warning(this, tr("Add Rule"), error);
    return;
  }
  SharedRulesStore::addRule(setName, obj);
}

void ManageRuleSetsDialog::onSetAdd() {
  bool          ok   = false;
  const QString name = QInputDialog::getText(this,
                                             tr("Add Rule Set"),
                                             tr("Rule set name"),
                                             QLineEdit::Normal,
                                             "default",
                                             &ok)
                           .trimmed();
  if (!ok || name.isEmpty()) {
    return;
  }
  if (!SharedRulesStore::ensureRuleSet(name)) {
    QMessageBox::warning(
        this, tr("Add Rule Set"), tr("Failed to create rule set."));
    return;
  }
  reload();
  emit ruleSetsChanged();
}

void ManageRuleSetsDialog::onSetRename() {
  const QString current = selectedName();
  if (current.isEmpty() || current == "default") {
    return;
  }
  bool          ok   = false;
  const QString name = QInputDialog::getText(this,
                                             tr("Rename Rule Set"),
                                             tr("New name"),
                                             QLineEdit::Normal,
                                             current,
                                             &ok)
                           .trimmed();
  if (!ok || name.isEmpty() || name == current) {
    return;
  }
  if (!SharedRulesStore::renameRuleSet(current, name)) {
    QMessageBox::warning(
        this, tr("Rename Rule Set"), tr("Failed to rename rule set."));
    return;
  }
  reload();
  emit ruleSetsChanged();
}

void ManageRuleSetsDialog::onSetDelete() {
  const QString current = selectedName();
  if (current.isEmpty() || current == "default") {
    return;
  }
  if (!confirmDelete(current)) {
    return;
  }
  if (!SharedRulesStore::removeRuleSet(current)) {
    QMessageBox::warning(
        this, tr("Delete Rule Set"), tr("Failed to delete rule set."));
    return;
  }
  reload();
  emit ruleSetsChanged();
}

void ManageRuleSetsDialog::onSetChanged() {
  reloadRules();
}

void ManageRuleSetsDialog::onRuleAdd() {
  const QString set = selectedName();
  if (set.isEmpty()) {
    return;
  }
  QString     error;
  QStringList outboundTags =
      RuleConfigService::loadOutboundTags(m_configRepo, "direct", &error);
  if (!error.isEmpty()) {
    QMessageBox::warning(this, tr("Add Rule"), error);
    return;
  }
  RuleEditorDialog dlg(RuleEditorDialog::Mode::Add, this);
  dlg.setOutboundTags(outboundTags);
  dlg.setRuleSetName(set);
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }
  RuleConfigService::RuleEditData data = dlg.editData();
  data.ruleSet                         = set;
  addRuleToSet(set, data);
  reloadRules();
  emit ruleSetsChanged();
}

void ManageRuleSetsDialog::onRuleDelete() {
  const QString set = selectedName();
  if (set.isEmpty()) {
    return;
  }
  QList<QListWidgetItem*> items = m_ruleList->selectedItems();
  if (items.isEmpty()) {
    return;
  }
  for (QListWidgetItem* it : items) {
    QJsonObject obj = it->data(Qt::UserRole).toJsonObject();
    SharedRulesStore::removeRule(set, obj);
  }
  reloadRules();
  emit ruleSetsChanged();
}

void ManageRuleSetsDialog::onSetContextMenu(const QPoint& pos) {
  const QString set     = selectedName();
  const bool    hasSet  = !set.isEmpty();
  const bool    canEdit = hasSet && set != "default";
  RoundedMenu   menu(this);
  menu.setObjectName("TrayMenu");
  ThemeService* ts = m_themeService;
  if (ts) {
    menu.setThemeColors(ts->color("bg-secondary"), ts->color("primary"));
    connect(ts, &ThemeService::themeChanged, &menu, [&menu, ts]() {
      menu.setThemeColors(ts->color("bg-secondary"), ts->color("primary"));
    });
  }
  QAction* addAct    = menu.addAction(tr("Add"));
  QAction* renameAct = menu.addAction(tr("Rename"));
  QAction* delAct    = menu.addAction(tr("Delete"));
  renameAct->setEnabled(canEdit);
  delAct->setEnabled(canEdit);
  QAction* chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
  if (!chosen) {
    return;
  }
  if (chosen == addAct) {
    onSetAdd();
  } else if (chosen == renameAct) {
    onSetRename();
  } else if (chosen == delAct) {
    onSetDelete();
  }
}

void ManageRuleSetsDialog::onRuleContextMenu(const QPoint& pos) {
  const QString set = selectedName();
  if (set.isEmpty()) {
    return;
  }
  const bool  hasRule = !m_ruleList->selectedItems().isEmpty();
  RoundedMenu menu(this);
  menu.setObjectName("TrayMenu");
  ThemeService* ts = m_themeService;
  if (ts) {
    menu.setThemeColors(ts->color("bg-secondary"), ts->color("primary"));
    connect(ts, &ThemeService::themeChanged, &menu, [&menu, ts]() {
      menu.setThemeColors(ts->color("bg-secondary"), ts->color("primary"));
    });
  }
  QAction* addAct = menu.addAction(tr("Add Rule"));
  QAction* delAct = menu.addAction(tr("Delete Rule"));
  delAct->setEnabled(hasRule);
  QAction* chosen = menu.exec(m_ruleList->viewport()->mapToGlobal(pos));
  if (!chosen) {
    return;
  }
  if (chosen == addAct) {
    onRuleAdd();
  } else if (chosen == delAct) {
    onRuleDelete();
  }
}
