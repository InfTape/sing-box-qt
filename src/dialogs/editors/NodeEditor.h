#ifndef NODEEDITOR_H
#define NODEEDITOR_H
#include <QJsonObject>
#include <QWidget>

class NodeEditor : public QWidget {
  Q_OBJECT
 public:
  explicit NodeEditor(QWidget* parent = nullptr) : QWidget(parent) {}

  virtual ~NodeEditor()                                        = default;
  virtual QJsonObject getOutbound() const                      = 0;
  virtual void        setOutbound(const QJsonObject& outbound) = 0;
  virtual bool        validate(QString* error) const           = 0;
 signals:
  void dataChanged();
};
#endif  // NODEEDITOR_H
