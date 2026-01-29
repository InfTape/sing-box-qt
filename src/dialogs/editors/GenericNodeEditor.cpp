#include "GenericNodeEditor.h"
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QGroupBox>
#include <QJsonArray>


GenericNodeEditor::GenericNodeEditor(const QString &type, QWidget *parent)
    : NodeEditor(parent)
    , m_type(type)
{
    setupUI();
    updateVisibility();
}

void GenericNodeEditor::setupUI()
{
    QFormLayout *layout = new QFormLayout(this);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    // Common
    m_nameEdit = new QLineEdit;
    m_serverEdit = new QLineEdit;
    m_portScan = new QSpinBox;
    m_portScan->setRange(1, 65535);
    m_portScan->setValue(443);
    m_portScan->setButtonSymbols(QAbstractSpinBox::NoButtons);

    layout->addRow(tr("Name"), m_nameEdit);
    layout->addRow(tr("Server"), m_serverEdit);
    layout->addRow(tr("Port"), m_portScan);

    // Auth
    m_uuidEdit = new QLineEdit;
    m_passwordEdit = new QLineEdit;
    m_passwordEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    m_alterIdEdit = new QLineEdit;
    m_alterIdEdit->setText("0");
    m_methodEdit = new QLineEdit;
    m_securityEdit = new QLineEdit;

    if (m_type == "vmess" || m_type == "vless" || m_type == "tuic" || m_type == "hysteria2") {
       layout->addRow(m_type == "tuic" ? tr("UUID") : (m_type == "hysteria2" ? tr("Password") : tr("UUID")), m_type == "hysteria2" ? m_passwordEdit : m_uuidEdit);
    }
    if (m_type == "shadowsocks" || m_type == "trojan" || m_type == "tuic") {
        layout->addRow(tr("Password"), m_passwordEdit);
    }
    if (m_type == "shadowsocks") {
        layout->addRow(tr("Method"), m_methodEdit);
    }
    if (m_type == "vmess") {
        layout->addRow(tr("Security"), m_securityEdit);
        layout->addRow(tr("AlterId"), m_alterIdEdit);
    }
    if (m_type == "vless") {
        m_flowCombo = new MenuComboBox;
        m_flowCombo->addItems({tr("None"), "xtls-rprx-vision", "xtls-rprx-direct"});
        m_flowCombo->setCurrentIndex(0);
        m_flowCombo->setWheelEnabled(false);
        layout->addRow(tr("Flow"), m_flowCombo);
    }

    // Transport / Stream Settings
    if (m_type == "vmess" || m_type == "vless" || m_type == "trojan" || m_type == "shadowsocks" || m_type == "hysteria2") {
        QGroupBox *transportGroup = new QGroupBox(tr("Transport && Security"));
        QFormLayout *transportLayout = new QFormLayout(transportGroup);

        m_networkCombo = new MenuComboBox;
        m_networkCombo->addItems({"tcp", "ws", "grpc", "http"}); 
        m_networkCombo->setWheelEnabled(false);
        transportLayout->addRow(tr("Network"), m_networkCombo);

        if (m_type == "vless") {
            m_securityCombo = new MenuComboBox;
            m_securityCombo->addItems({"none", "tls", "reality"});
            m_securityCombo->setCurrentText("tls");
            m_securityCombo->setWheelEnabled(false);
            transportLayout->addRow(tr("Security"), m_securityCombo);
        } else {
            m_securityCombo = nullptr;
        }

        m_tlsCheck = new QCheckBox(tr("TLS"));
        if (m_type == "vless") {
            m_tlsCheck->setChecked(true);
        }
        transportLayout->addRow(tr("Security"), m_tlsCheck);

        m_serverNameEdit = new QLineEdit;
        m_alpnEdit = new QLineEdit;
        m_fingerprintEdit = new QLineEdit;
        m_insecureCheck = new QCheckBox(tr("Allow Insecure"));
        
        transportLayout->addRow(tr("SNI"), m_serverNameEdit);
        transportLayout->addRow(tr("ALPN"), m_alpnEdit);
        transportLayout->addRow(tr("Fingerprint"), m_fingerprintEdit);
        transportLayout->addRow(tr(""), m_insecureCheck);

        // Path/Host/ServiceName
        m_pathEdit = new QLineEdit;
        m_hostEdit = new QLineEdit;
        m_serviceNameEdit = new QLineEdit;
        transportLayout->addRow(tr("Path"), m_pathEdit);
        transportLayout->addRow(tr("Host"), m_hostEdit);
        transportLayout->addRow(tr("Service Name"), m_serviceNameEdit);

        // Reality
        if (m_type == "vless" || m_type == "trojan") {
             m_publicKeyEdit = new QLineEdit;
             m_shortIdEdit = new QLineEdit;
             m_spiderXEdit = new QLineEdit;
             transportLayout->addRow(tr("Reality Public Key"), m_publicKeyEdit);
             transportLayout->addRow(tr("Reality Short ID"), m_shortIdEdit);
             transportLayout->addRow(tr("Reality SpiderX"), m_spiderXEdit);
        }

        layout->addRow(transportGroup);
        
        connect(m_networkCombo, &QComboBox::currentTextChanged, this, &GenericNodeEditor::updateVisibility);
        connect(m_tlsCheck, &QCheckBox::toggled, this, &GenericNodeEditor::updateVisibility);
        if (m_securityCombo) {
            connect(m_securityCombo, &QComboBox::currentTextChanged, this, &GenericNodeEditor::updateVisibility);
        }
    }

    // Connect signals
    QList<QWidget*> widgets = findChildren<QWidget*>();
    for (auto w : widgets) {
        if (qobject_cast<QLineEdit*>(w)) {
            connect(qobject_cast<QLineEdit*>(w), &QLineEdit::textChanged, this, &NodeEditor::dataChanged);
        } else if (qobject_cast<QSpinBox*>(w)) {
            connect(qobject_cast<QSpinBox*>(w), QOverload<int>::of(&QSpinBox::valueChanged), this, &NodeEditor::dataChanged);
        } else if (qobject_cast<QCheckBox*>(w)) {
            connect(qobject_cast<QCheckBox*>(w), &QCheckBox::toggled, this, &NodeEditor::dataChanged);
        } else if (qobject_cast<QComboBox*>(w)) {
            connect(qobject_cast<QComboBox*>(w), &QComboBox::currentTextChanged, this, &NodeEditor::dataChanged);
        }
    }
}


void GenericNodeEditor::updateVisibility()
{
    if (m_networkCombo) {
        QString net = m_networkCombo->currentText();
        bool isWs = net == "ws";
        bool isGrpc = net == "grpc";
        bool isHttp = net == "http";

        QFormLayout *layout = nullptr;
        if (m_pathEdit && m_pathEdit->parentWidget()) {
            layout = qobject_cast<QFormLayout*>(m_pathEdit->parentWidget()->layout());
        }
        if (layout) {
            auto safeSetVisible = [layout](QWidget *field, bool visible) {
                if (!field) return;
                if (layout->indexOf(field) < 0) return; // not part of this form layout
                field->setVisible(visible);
                if (QWidget *lbl = layout->labelForField(field)) {
                    lbl->setVisible(visible);
                }
            };

            safeSetVisible(m_pathEdit, isWs || isHttp);
            safeSetVisible(m_hostEdit, isWs || isHttp);
            safeSetVisible(m_serviceNameEdit, isGrpc);

            // Reality fields只在 security=reality 时显示
            const bool isReality = m_securityCombo && m_securityCombo->currentText() == "reality";
            safeSetVisible(m_publicKeyEdit, isReality);
            safeSetVisible(m_shortIdEdit, isReality);
            safeSetVisible(m_spiderXEdit, isReality);
        }
    }

    // security=none 时自动取消 TLS 选项
    if (m_securityCombo && m_tlsCheck) {
        const QString sec = m_securityCombo->currentText();
        if (sec == "none") {
            m_tlsCheck->setChecked(false);
            m_tlsCheck->setEnabled(false);
        } else {
            if (sec == "tls" || sec == "reality") {
                m_tlsCheck->setChecked(true);
            }
            m_tlsCheck->setEnabled(true);
        }
    }
}

QJsonObject GenericNodeEditor::getOutbound() const
{
    QJsonObject outbound;
    outbound["type"] = m_type;
    outbound["tag"] = m_nameEdit->text();
    outbound["server"] = m_serverEdit->text();
    outbound["server_port"] = m_portScan->value();

    if (m_type == "vmess") {
        outbound["uuid"] = m_uuidEdit->text();
        // VMess security 默认 auto，避免空字符串导致配置无效
        const QString sec = m_securityEdit->text().trimmed();
        outbound["security"] = sec.isEmpty() ? "auto" : sec;
        outbound["alter_id"] = m_alterIdEdit->text().toInt();
    } else if (m_type == "vless") {
        outbound["uuid"] = m_uuidEdit->text();
        const QString flow = m_flowCombo ? m_flowCombo->currentText() : QString();
        if (!flow.isEmpty() && flow.toLower() != tr("none").toLower()) {
            outbound["flow"] = flow;
        }
    } else if (m_type == "shadowsocks") {
        outbound["method"] = m_methodEdit->text();
        outbound["password"] = m_passwordEdit->text();
    } else if (m_type == "trojan") {
        outbound["password"] = m_passwordEdit->text();
    } else if (m_type == "tuic") {
        outbound["uuid"] = m_uuidEdit->text();
        outbound["password"] = m_passwordEdit->text();
    } else if (m_type == "hysteria2") {
        outbound["password"] = m_passwordEdit->text();
    }

    // Hysteria2 强制启用 TLS，需提供 SNI
    if (m_type == "hysteria2") {
        QJsonObject tls;
        tls["enabled"] = true;
        const QString sni = m_serverNameEdit ? m_serverNameEdit->text().trimmed() : QString();
        tls["server_name"] = sni.isEmpty() ? m_serverEdit->text().trimmed() : sni; // 空则回落到服务器域名
        if (m_insecureCheck) {
            tls["insecure"] = m_insecureCheck->isChecked();
        }
        outbound["tls"] = tls;
    }

    // Transport & TLS
    if (m_networkCombo && m_type != "hysteria2") {
        QString net = m_networkCombo->currentText().trimmed().toLower();
        // 对于 tcp（默认），不写 transport，避免出现 unknown transport type: tcp
        if (!net.isEmpty() && net != "tcp") {
            QJsonObject transport;
            transport["type"] = net;
            
            if (net == "ws") {
                 transport["path"] = m_pathEdit->text();
                 if (!m_hostEdit->text().isEmpty()) {
                     transport["headers"] = QJsonObject{{"Host", m_hostEdit->text()}};
                 }
            } else if (net == "grpc") {
                 transport["service_name"] = m_serviceNameEdit->text();
            } else if (net == "http") {
                 transport["path"] = m_pathEdit->text();
                 if (!m_hostEdit->text().isEmpty()) {
                     transport["host"] = QJsonArray{m_hostEdit->text()};
                 }
            }
            outbound["transport"] = transport;
        }
    }

    const QString vlessSecurity = m_securityCombo ? m_securityCombo->currentText() : QString();

    auto lineText = [](QLineEdit *w) -> QString { return w ? w->text() : QString(); };
    auto comboText = [](QComboBox *c) -> QString { return c ? c->currentText() : QString(); };

    // 启用 TLS 的条件：勾选、明确 security=tls/reality，或 Reality 字段非空
    const bool hasRealityField = m_publicKeyEdit && !m_publicKeyEdit->text().isEmpty();
    const bool shouldEnableTls = (m_tlsCheck && m_tlsCheck->isChecked())
        || (vlessSecurity == "reality")
        || (vlessSecurity == "tls")
        || hasRealityField;

    if (shouldEnableTls && m_serverNameEdit && m_networkCombo) {
        QJsonObject tls;
        tls["enabled"] = true;
        
        QString sni = lineText(m_serverNameEdit);
        if (sni.isEmpty() && m_networkCombo->isVisible()) {
            // If SNI is empty, try to use Host/ServiceName as fallback
            const QString net = comboText(m_networkCombo);
            if ((net == "ws" || net == "http") && !lineText(m_hostEdit).isEmpty()) {
                sni = lineText(m_hostEdit);
            } else if (net == "grpc" && !lineText(m_serviceNameEdit).isEmpty()) {
                sni = lineText(m_serviceNameEdit);
            }
        }
        tls["server_name"] = sni;
        
        if (m_insecureCheck) {
            tls["insecure"] = m_insecureCheck->isChecked();
        }
        if (m_alpnEdit && !m_alpnEdit->text().isEmpty()) {
            // tls["alpn"] = QJsonArray::fromStringList(m_alpnEdit->text().split(","));
             // Simplified
        }
        if ((vlessSecurity == "reality") || hasRealityField) {
             QJsonObject reality;
             reality["enabled"] = true;
             if (m_publicKeyEdit) reality["public_key"] = m_publicKeyEdit->text();
             if (m_shortIdEdit) reality["short_id"] = m_shortIdEdit->text();
             tls["reality"] = reality;
        }
        if (m_fingerprintEdit && !m_fingerprintEdit->text().isEmpty()) {
             QJsonObject utls;
             utls["enabled"] = true;
             utls["fingerprint"] = m_fingerprintEdit->text();
             tls["utls"] = utls;
        }
        outbound["tls"] = tls;
    }

    return outbound;
}

void GenericNodeEditor::setOutbound(const QJsonObject &outbound)
{
    m_nameEdit->setText(outbound["tag"].toString());
    m_serverEdit->setText(outbound["server"].toString());
    m_portScan->setValue(outbound["server_port"].toInt());

    if (m_type == "vmess") {
        m_uuidEdit->setText(outbound["uuid"].toString());
        m_securityEdit->setText(outbound["security"].toString());
        m_alterIdEdit->setText(QString::number(outbound["alter_id"].toInt()));
    } else if (m_type == "vless") {
        m_uuidEdit->setText(outbound["uuid"].toString());
        if (m_flowCombo) {
            const QString flow = outbound["flow"].toString();
            if (!flow.isEmpty()) {
                int idx = m_flowCombo->findText(flow);
                if (idx >= 0) m_flowCombo->setCurrentIndex(idx);
            }
        }
    } else if (m_type == "shadowsocks") {
        m_methodEdit->setText(outbound["method"].toString());
        m_passwordEdit->setText(outbound["password"].toString());
    } else if (m_type == "trojan") {
        m_passwordEdit->setText(outbound["password"].toString());
    } else if (m_type == "tuic") {
        m_uuidEdit->setText(outbound["uuid"].toString());
        m_passwordEdit->setText(outbound["password"].toString());
    } else if (m_type == "hysteria2") {
        m_passwordEdit->setText(outbound["password"].toString());
    }

    if (outbound.contains("transport")) {
        QJsonObject t = outbound["transport"].toObject();
        m_networkCombo->setCurrentText(t["type"].toString());
        m_pathEdit->setText(t["path"].toString());
        m_serviceNameEdit->setText(t["service_name"].toString());
        
        // Handle loading host
        if (t.contains("headers")) {
             QJsonObject headers = t["headers"].toObject();
             if (headers.contains("Host")) {
                 m_hostEdit->setText(headers["Host"].toString());
             }
        } else if (t.contains("host")) {
             QJsonArray hosts = t["host"].toArray();
             if (!hosts.isEmpty()) {
                 m_hostEdit->setText(hosts[0].toString());
             }
        }
    }

    QString detectedSecurity;
    if (outbound.contains("tls")) {
        QJsonObject t = outbound["tls"].toObject();
        m_tlsCheck->setChecked(t["enabled"].toBool());
        m_serverNameEdit->setText(t["server_name"].toString());
        m_insecureCheck->setChecked(t["insecure"].toBool());
        
        if (t.contains("reality")) {
            QJsonObject r = t["reality"].toObject();
            m_publicKeyEdit->setText(r["public_key"].toString());
            m_shortIdEdit->setText(r["short_id"].toString());
            detectedSecurity = "reality";
        }
        
        if (t.contains("utls")) {
             QJsonObject u = t["utls"].toObject();
             m_fingerprintEdit->setText(u["fingerprint"].toString());
        }
        if (detectedSecurity.isEmpty() && t.value("enabled").toBool()) {
            detectedSecurity = "tls";
        }
    } else {
        detectedSecurity = "none";
    }

    if (m_securityCombo) {
        if (!detectedSecurity.isEmpty()) {
            m_securityCombo->setCurrentText(detectedSecurity);
        } else {
            m_securityCombo->setCurrentText("tls");
        }
    }
}

bool GenericNodeEditor::validate(QString *error) const
{
    if (m_serverEdit->text().isEmpty()) {
        if (error) *error = tr("Server address is required");
        return false;
    }
    return true;
}
