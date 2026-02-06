#ifndef THEMEPROVIDER_H
#define THEMEPROVIDER_H
#include "app/interfaces/ThemeService.h"
/**
 * @brief ThemeProvider provides global ThemeService access point, replacing direct dependency on ThemeManager
 * Singleton.
 *
 * Usage:
 * - Call ThemeProvider::setInstance(themeService) when initializing AppContext
 * - UI components use ThemeProvider::instance()->color("key") methods
 */
class ThemeProvider {
 public:
  static ThemeService* instance();
  static void          setInstance(ThemeService* service);

 private:
  static ThemeService* s_instance;
};
#endif  // THEMEPROVIDER_H
