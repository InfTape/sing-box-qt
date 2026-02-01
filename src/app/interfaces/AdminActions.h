#ifndef ADMINACTIONS_H
#define ADMINACTIONS_H
class AdminActions {
 public:
  virtual ~AdminActions()       = default;
  virtual bool isAdmin() const  = 0;
  virtual bool restartAsAdmin() = 0;
};
#endif  // ADMINACTIONS_H
