#ifndef ADMINHELPER_H
#define ADMINHELPER_H
#include <QObject>
#include <QString>
#include <QStringList>

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
  static bool runAsAdminAndWait(const QString&     program,
                                const QStringList& arguments = QStringList(),
                                int                timeoutMs = 30000);
  // Linux TUN support without running the whole GUI as root.
  static bool hasTunPermission(const QString& program);
  static bool grantTunPermission(const QString& program);
};
#endif  // ADMINHELPER_H
