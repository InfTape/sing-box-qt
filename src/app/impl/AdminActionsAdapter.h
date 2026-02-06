#ifndef ADMINACTIONADAPTER_H
#define ADMINACTIONADAPTER_H
#include "app/interfaces/AdminActions.h"

class AdminActionsAdapter : public AdminActions {
 public:
  bool isAdmin() const override;
  bool restartAsAdmin() override;
};
#endif  // ADMINACTIONADAPTER_H
