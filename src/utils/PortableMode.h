#ifndef PORTABLEMODE_H
#define PORTABLEMODE_H
#include <QString>

namespace PortableMode {
bool    isPortableEnabled();
QString appRootDir();
QString portableDataDir();
}  // namespace PortableMode

#endif  // PORTABLEMODE_H
