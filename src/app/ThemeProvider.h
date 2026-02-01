#ifndef THEMEPROVIDER_H
#define THEMEPROVIDER_H

#include "app/interfaces/ThemeService.h"
/**
 * @brief ThemeProvider 提供全局 ThemeService 访问点，替代直接依赖 ThemeManager
 * 单例。
 *
 * 使用方式：
 * - 在 AppContext 初始化时调用 ThemeProvider::setInstance(themeService)
 * - UI 组件使用 ThemeProvider::instance()->color("key") 等方法
 */
class ThemeProvider {
 public:
  static ThemeService* instance();
  static void          setInstance(ThemeService* service);

 private:
  static ThemeService* s_instance;
};
#endif  // THEMEPROVIDER_H
