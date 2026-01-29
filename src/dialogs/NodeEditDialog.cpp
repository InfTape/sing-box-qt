#include "NodeEditDialog.h"
#include "editors/GenericNodeEditor.h"
#include "widgets/MenuComboBox.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QTextEdit>
#include <QJsonDocument>
#include <QMessageBox>
#include <QScrollArea>

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
    typeLayout->addStretch();
    mainLayout->addLayout(typeLayout);

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
}

QJsonObject NodeEditDialog::nodeData() const
{
    if (m_currentEditor) {
        return m_currentEditor->getOutbound();
    }
    return QJsonObject();
}
