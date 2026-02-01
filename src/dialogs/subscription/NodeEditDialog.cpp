#include "dialogs/subscription/NodeEditDialog.h"
#include "dialogs/editors/GenericNodeEditor.h"
#include "widgets/common/MenuComboBox.h"
#include "widgets/common/RoundedMenu.h"
#include "services/rules/SharedRulesStore.h"
#include "app/ThemeProvider.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QTextEdit>
#include <QJsonDocument>
#include <QMessageBox>
#include <QScrollArea>
#include <QCheckBox>
#include <QToolButton>
#include <QAction>
#include <QMetaObject>

NodeEditDialog::NodeEditDialog(QWidget *parent)
    : QDialog(parent)
    , m_currentEditor(nullptr)
    , m_scrollArea(nullptr)
    , m_editorPage(nullptr)
{
    setupUI();
}

NodeEditDialog::~NodeEditDialog()
{
}

void NodeEditDialog::setupUI()
{
    setWindowTitle(tr("Edit Node"));
    setMinimumWidth(600);
    setMinimumHeight(500);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Type selection
    QHBoxLayout *typeLayout = new QHBoxLayout;
    typeLayout->addWidget(new QLabel(tr("Type:")));
    m_typeCombo = new MenuComboBox;
    m_typeCombo->addItems({"vmess", "vless", "shadowsocks", "trojan", "tuic", "hysteria2"});
    m_typeCombo->setWheelEnabled(false);
    typeLayout->addWidget(m_typeCombo);

    // Rule set controls
    m_sharedRulesCheck = new QCheckBox(tr("Enable shared rule set"));
    m_sharedRulesCheck->setChecked(true);
    typeLayout->addWidget(m_sharedRulesCheck);

    m_ruleSetsBtn = new QToolButton(this);
    m_ruleSetsBtn->setText(tr("default"));
    m_ruleSetsBtn->setPopupMode(QToolButton::InstantPopup);
    m_ruleSetsBtn->setAutoRaise(true);
    typeLayout->addWidget(m_ruleSetsBtn);

    typeLayout->addStretch();
    mainLayout->addLayout(typeLayout);

    // Build rule set menu
    auto rebuildMenu = [this]() {
        if (QMenu *old = m_ruleSetsBtn->menu()) {
            old->deleteLater();
        }

        RoundedMenu *menu = new RoundedMenu(this);
        // 采用托盘菜单的样式，复用勾选外观
        menu->setObjectName("TrayMenu");
        ThemeService *ts = ThemeProvider::instance();
        if (ts) menu->setThemeColors(ts->color("bg-secondary"), ts->color("primary"));

        QStringList options = SharedRulesStore::listRuleSets();
        for (const auto &name : m_ruleSets) if (!options.contains(name)) options << name;
        options.removeDuplicates();
        options.removeAll(QString());
        if (!options.contains("default")) options.prepend("default");

        for (const QString &name : options) {
            QAction *act = menu->addAction(name);
            act->setCheckable(true);
            act->setChecked(m_ruleSets.contains(name) || (m_ruleSets.isEmpty() && name == "default"));
            connect(act, &QAction::triggered, this, [this, name, act]() {
                if (act->isChecked()) {
                    if (!m_ruleSets.contains(name)) m_ruleSets << name;
                } else {
                    m_ruleSets.removeAll(name);
                }
                if (m_ruleSets.isEmpty()) m_ruleSets << "default";
                m_ruleSets.removeDuplicates();
                m_ruleSetsBtn->setText(m_ruleSets.join(", "));
            });
        }
        m_ruleSetsBtn->setMenu(menu);
    };
    m_ruleSets = QStringList{"default"};
    rebuildMenu();
    connect(m_ruleSetsBtn, &QToolButton::clicked, this, rebuildMenu);
    connect(m_sharedRulesCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_ruleSetsBtn->setEnabled(on);
    });

    // Tabs
    m_tabs = new QTabWidget;
    mainLayout->addWidget(m_tabs);

    // Editor Tab
    QWidget *editorTab = new QWidget;
    QVBoxLayout *editorLayout = new QVBoxLayout(editorTab);
    editorLayout->setContentsMargins(0, 10, 0, 0);

    m_scrollArea = new QScrollArea(editorTab);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameStyle(QFrame::NoFrame);

    m_editorPage = new QWidget;
    m_editorContainer = new QVBoxLayout(m_editorPage);
    m_editorContainer->setContentsMargins(0, 0, 0, 0);
    m_editorContainer->setSpacing(12);
    m_editorContainer->addStretch();

    m_scrollArea->setWidget(m_editorPage);
    editorLayout->addWidget(m_scrollArea);
    m_tabs->addTab(editorTab, tr("Settings"));

    // Preview Tab
    m_jsonPreview = new QTextEdit;
    m_jsonPreview->setReadOnly(true);
    m_tabs->addTab(m_jsonPreview, tr("JSON Preview"));

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout;
    QPushButton *cancelBtn = new QPushButton(tr("Cancel"));
    QPushButton *okBtn = new QPushButton(tr("OK"));
    btnLayout->addStretch();
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(okBtn);
    mainLayout->addLayout(btnLayout);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_typeCombo, &QComboBox::currentTextChanged, this, &NodeEditDialog::onTypeChanged);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 1) updatePreview();
    });

    // Initial editor
    onTypeChanged(m_typeCombo->currentText());
}

NodeEditor* NodeEditDialog::createEditor(const QString &type)
{
    // For now, GenericNodeEditor handles all
    return new GenericNodeEditor(type, this);
}

void NodeEditDialog::onTypeChanged(const QString &type)
{
    if (m_currentEditor) {
        m_editorContainer->removeWidget(m_currentEditor);
        m_currentEditor->deleteLater();
    }

    m_currentEditor = createEditor(type);
    if (m_currentEditor) {
        // 插入到 stretch 之前，保持底部留白
        int stretchIndex = m_editorContainer->count() - 1;
        if (stretchIndex < 0) stretchIndex = 0;
        m_editorContainer->insertWidget(stretchIndex, m_currentEditor);
        // Connect data changed to nothing for now, or live preview if needed
    }
}

void NodeEditDialog::updatePreview()
{
    if (m_currentEditor) {
        QJsonObject obj = m_currentEditor->getOutbound();
        m_jsonPreview->setText(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    }
}

void NodeEditDialog::setNodeData(const QJsonObject &node)
{
    QString type = node["type"].toString();
    int index = m_typeCombo->findText(type);
    if (index >= 0) {
        m_typeCombo->setCurrentIndex(index);
    }
    
    // Force editor creation if type didn't change but we need to set data
    // (Actually setCurrentIndex triggers signal if changed, but we should be safe)
    if (!m_currentEditor || m_typeCombo->currentText() != type) {
         onTypeChanged(type);
    }

    if (m_currentEditor) {
        m_currentEditor->setOutbound(node);
    }

    // restore rule set selections if provided in node comment/tag? not present, keep current
}

void NodeEditDialog::setRuleSets(const QStringList &sets, bool enableShared)
{
    m_sharedRulesCheck->setChecked(enableShared);
    m_ruleSets = sets;
    if (m_ruleSets.isEmpty()) m_ruleSets << "default";
    m_ruleSets.removeDuplicates();
    m_ruleSetsBtn->setText(m_ruleSets.join(", "));
    // rebuild menu to reflect selections
    QMetaObject::invokeMethod(m_ruleSetsBtn, [this, enableShared]() {
        if (m_ruleSetsBtn->menu()) m_ruleSetsBtn->menu()->deleteLater();
        // rebuild
        RoundedMenu *menu = new RoundedMenu(this);
        menu->setObjectName("TrayMenu");
        ThemeService *ts = ThemeProvider::instance();
        if (ts) menu->setThemeColors(ts->color("bg-secondary"), ts->color("primary"));

        QStringList options = SharedRulesStore::listRuleSets();
        for (const auto &name : m_ruleSets) if (!options.contains(name)) options << name;
        options.removeDuplicates();
        options.removeAll(QString());
        if (!options.contains("default")) options.prepend("default");

        for (const QString &name : options) {
            QAction *act = menu->addAction(name);
            act->setCheckable(true);
            act->setChecked(m_ruleSets.contains(name));
            connect(act, &QAction::triggered, this, [this, name, act]() {
                if (act->isChecked()) {
                    if (!m_ruleSets.contains(name)) m_ruleSets << name;
                } else {
                    m_ruleSets.removeAll(name);
                }
                if (m_ruleSets.isEmpty()) m_ruleSets << "default";
                m_ruleSets.removeDuplicates();
                m_ruleSetsBtn->setText(m_ruleSets.join(", "));
            });
        }
        m_ruleSetsBtn->setMenu(menu);
        m_ruleSetsBtn->setEnabled(enableShared);
    }, Qt::QueuedConnection);
}

QJsonObject NodeEditDialog::nodeData() const
{
    if (m_currentEditor) {
        return m_currentEditor->getOutbound();
    }
    return QJsonObject();
}

QStringList NodeEditDialog::ruleSets() const
{
    if (m_ruleSets.isEmpty()) return {"default"};
    return m_ruleSets;
}

bool NodeEditDialog::sharedRulesEnabled() const
{
    return m_sharedRulesCheck && m_sharedRulesCheck->isChecked();
}
