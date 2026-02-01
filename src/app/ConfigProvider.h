#ifndef CONFIGPROVIDER_H
#define CONFIGPROVIDER_H

#include "app/interfaces/ConfigRepository.h"
class ConfigProvider {
 public:
  static void              setInstance(ConfigRepository* repo);
  static ConfigRepository* instance();

 private:
  static ConfigRepository* s_instance;
};
#endif  // CONFIGPROVIDER_H
