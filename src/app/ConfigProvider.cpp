#include "ConfigProvider.h"
ConfigRepository* ConfigProvider::s_instance = nullptr;
void              ConfigProvider::setInstance(ConfigRepository* repo) {
  s_instance = repo;
}
ConfigRepository* ConfigProvider::instance() {
  return s_instance;
}
