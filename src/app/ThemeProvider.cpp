#include "ThemeProvider.h"

ThemeService* ThemeProvider::s_instance = nullptr;
ThemeService* ThemeProvider::instance() {
  return s_instance;
}
void ThemeProvider::setInstance(ThemeService* service) {
  s_instance = service;
}
