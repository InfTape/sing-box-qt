#ifndef GENERICNODEEDITOR_H
#define GENERICNODEEDITOR_H

#include "NodeEditor.h"
#include "widgets/common/MenuComboBox.h"

class QLineEdit;
class QSpinBox;
class QCheckBox;
class QFormLayout;
class GenericNodeEditor : public NodeEditor {
  Q_OBJECT
 public:
  explicit GenericNodeEditor(const QString& type, QWidget* parent = nullptr);

  QJsonObject getOutbound() const override;
  void        setOutbound(const QJsonObject& outbound) override;
  bool        validate(QString* error) const override;

 private:
  void setupUI();
  void updateVisibility();

  QString m_type;

  // Common
  QLineEdit* m_nameEdit;
  QLineEdit* m_serverEdit = nullptr;
  QSpinBox*  m_portScan   = nullptr;

  // VMess/VLESS
  QLineEdit*    m_uuidEdit     = nullptr;
  QLineEdit*    m_securityEdit = nullptr;  // VMess security / VLESS flow? No, flow is flow.
  MenuComboBox* m_flowCombo    = nullptr;  // VLESS flow
  QLineEdit*    m_alterIdEdit  = nullptr;  // VMess alterId (deprecated but maybe needed?)

  // Shadowsocks / Trojan / Tuic / Hysteria
  QLineEdit* m_methodEdit   = nullptr;  // SS method
  QLineEdit* m_passwordEdit = nullptr;

  // VMess/VLESS/Trojan Transport & TLS
  MenuComboBox* m_networkCombo    = nullptr;  // tcp, ws, grpc, etc.
  MenuComboBox* m_securityCombo   = nullptr;  // VLESS security: none/tls/reality
  QCheckBox*    m_tlsCheck        = nullptr;
  QLineEdit*    m_serverNameEdit  = nullptr;  // SNI
  QLineEdit*    m_alpnEdit        = nullptr;
  QCheckBox*    m_insecureCheck   = nullptr;
  QLineEdit*    m_fingerprintEdit = nullptr;
  QLineEdit*    m_publicKeyEdit   = nullptr;  // Reality
  QLineEdit*    m_shortIdEdit     = nullptr;  // Reality
  QLineEdit*    m_spiderXEdit     = nullptr;  // Reality

  // Transport specific
  QLineEdit* m_pathEdit        = nullptr;  // WS/HTTP/gRPC path
  QLineEdit* m_hostEdit        = nullptr;  // HTTP/WS Host
  QLineEdit* m_serviceNameEdit = nullptr;  // gRPC serviceName

  // Hysteria2
  QLineEdit* m_obfsTypeEdit     = nullptr;
  QLineEdit* m_obfsPasswordEdit = nullptr;
};
#endif  // GENERICNODEEDITOR_H
