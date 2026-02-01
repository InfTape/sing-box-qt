#ifndef ADMINHELPER_H
#define ADMINHELPER_H

#include <QObject>
#include <QString>
class AdminHelper : public QObject {
  Q_OBJECT

 public:
  explicit AdminHelper(QObject* parent = nullptr);

  // Check admin privileges.
  static bool isAdmin();

  // Restart the app as admin.
  static bool restartAsAdmin();

  // Request UAC elevation to run a command.
  static bool runAsAdmin(const QString&     program,
                         const QStringList& arguments = QStringList());
};
#endif  // ADMINHELPER_H
