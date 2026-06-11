#ifndef CORESERVICE_H
#define CORESERVICE_H

class CoreService {
 public:
  static bool isSupported();
  static bool isInstalled();
  static bool setEnabled(bool enabled);
};

#endif  // CORESERVICE_H
