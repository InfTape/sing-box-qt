#ifndef CONFIGIO_H
#define CONFIGIO_H
#include <QJsonObject>
#include <QString>
namespace ConfigIO {
QString     getConfigDir();
QString     getActiveConfigPath();
QJsonObject loadConfig(const QString& path);
bool        saveConfig(const QString& path, const QJsonObject& config);
}  // namespace ConfigIO
#endif  // CONFIGIO_H
