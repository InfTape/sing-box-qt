#ifndef RUNTIMECONFIGRESOLVER_H
#define RUNTIMECONFIGRESOLVER_H

#include <QString>

namespace RuntimeConfigResolver {
QString defaultConfigPath();
QString selectConfigPath(const QString& persistedConfigPath,
                         const QString& fallbackConfigPath);
QString resolveConfigPath();
}  // namespace RuntimeConfigResolver

#endif  // RUNTIMECONFIGRESOLVER_H
