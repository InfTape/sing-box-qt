#ifndef GENERICNODEEDITOR_H
#define GENERICNODEEDITOR_H

#include "NodeEditor.h"
#include "widgets/MenuComboBox.h"

class QLineEdit;
class QSpinBox;
class QCheckBox;
class QFormLayout;

class GenericNodeEditor : public NodeEditor
{
    Q_OBJECT
public:
    explicit GenericNodeEditor(const QString &type, QWidget *parent = nullptr);

    QJsonObject getOutbound() const override;
    void setOutbound(const QJsonObject &outbound) override;
    bool validate(QString *error) const override;

private:
    void setupUI();
    void updateVisibility();

    QString m_type;

    // Common
    QLineEdit *m_nameEdit;
    QLineEdit *m_serverEdit;
    QSpinBox *m_portScan;
    
    // VMess/VLESS
    QLineEdit *m_uuidEdit;
    QLineEdit *m_securityEdit; // VMess security / VLESS flow? No, flow is flow.
    QLineEdit *m_flowEdit; // VLESS flow
    QLineEdit *m_alterIdEdit; // VMess alterId (deprecated but maybe needed?)

    // Shadowsocks / Trojan / Tuic / Hysteria
    QLineEdit *m_methodEdit; // SS method
    QLineEdit *m_passwordEdit; 

    // VMess/VLESS/Trojan Transport & TLS
    MenuComboBox *m_networkCombo; // tcp, ws, grpc, etc.
    MenuComboBox *m_securityCombo; // VLESS security: none/tls/reality
    QCheckBox *m_tlsCheck;
    QLineEdit *m_serverNameEdit; // SNI
    QLineEdit *m_alpnEdit;
    QCheckBox *m_insecureCheck;
    QLineEdit *m_fingerprintEdit;
    QLineEdit *m_publicKeyEdit; // Reality
    QLineEdit *m_shortIdEdit; // Reality
    QLineEdit *m_spiderXEdit; // Reality
    
    // Transport specific
    QLineEdit *m_pathEdit; // WS/HTTP/gRPC path
    QLineEdit *m_hostEdit; // HTTP/WS Host
    QLineEdit *m_serviceNameEdit; // gRPC serviceName

    // Hysteria2
    QLineEdit *m_obfsTypeEdit;
    QLineEdit *m_obfsPasswordEdit;
};

#endif // GENERICNODEEDITOR_H
