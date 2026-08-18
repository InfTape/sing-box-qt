#include "AdminActionsAdapter.h"
#include "services/kernel/KernelPlatform.h"
#include "system/AdminHelper.h"

bool AdminActionsAdapter::isAdmin() const {
#ifdef Q_OS_LINUX
  return AdminHelper::hasTunPermission(KernelPlatform::detectKernelPath());
#else
  return AdminHelper::isAdmin();
#endif
}

bool AdminActionsAdapter::restartAsAdmin() {
#ifdef Q_OS_LINUX
  return AdminHelper::grantTunPermission(KernelPlatform::detectKernelPath());
#else
  return AdminHelper::restartAsAdmin();
#endif
}
