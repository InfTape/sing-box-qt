#include "AdminActionsAdapter.h"

#include "system/AdminHelper.h"

bool AdminActionsAdapter::isAdmin() const
{
    return AdminHelper::isAdmin();
}

bool AdminActionsAdapter::restartAsAdmin()
{
    return AdminHelper::restartAsAdmin();
}
