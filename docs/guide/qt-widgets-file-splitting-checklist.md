# Qt Widgets 项目文件拆分 Checklist（实战可直接照做）

## 0. 先做 10 秒自检（是否需要拆）

- 单个 `.cpp` 超过 400 行（>600 基本必拆）
- 一个文件里出现两个及以上 `class` 定义
- 同时包含 UI 布局 + 业务逻辑 + 工具函数（≥2 种职责）
- 你需要频繁在文件内“搜函数名”才能找到入口
- 代码改动常常牵扯到很多不相关区域（改 A 时总碰到 B）

满足 2 条以上 → 建议拆分。

## 1. 拆分的第一原则（最重要）

- 一个文件只负责一种职责
- 可复用的东西不要放在页面/窗口文件里
- UI 展示组件不直接依赖 Service/网络/磁盘（只发 signal）
- 业务/数据层不 include UI 头文件（防止反向依赖）

## 2. 典型 Qt Widgets 分层（你照这个映射就行）

### A) Page / View（页面）

- 放在：`views/xxx/`
- 只做：`setupUI / applyTheme / refresh + slots`
- 负责把 Model/Service 的数据“组装成控件”
- 不要塞一堆工具函数/子类控件定义

### B) Component / Widget（可复用组件）

- 放在：`widgets/`
- 自绘按钮、卡片、导航项、空状态、Badge、Toast 等
- 只关心显示与交互
- 对外暴露 `property + signal`，不干业务

### C) Dialog（弹窗）

- 放在：`dialogs/` 或 `views/xxx/dialogs/`
- 只做输入/展示、校验、返回数据
- 提供 `setData()/data()` 或 getter
- 不直接调用 Service（由 Page 调）

### D) Model（数据模型）

- 放在：`models/`
- `QAbstractTableModel / QAbstractListModel`
- 只处理数据、角色、排序/过滤接口
- 不画 UI，不写 layout

### E) Delegate / Style（绘制与风格）

- 放在：`delegates/`、`style/`
- `QStyledItemDelegate` 只负责 paint/editor
- `QProxyStyle`、QSS 统一主题
- 这里通常是“高级感”的主要来源

### F) Service / UseCase（业务）

- 放在：`services/`、`network/`
- 网络请求、配置读写、缓存、定时刷新
- 不依赖 QWidget（最多用 QObject 信号）

### G) Utils（工具）

- 放在：`utils/`
- `formatBytes/isJsonText` 这种纯函数
- 尽量无状态、可测试

## 3. “看到这些就该拆”的强信号

- 文件里出现 `namespace { ... }` 放了很多工具函数（应挪到 utils）
- 文件里定义了 `class XxxDialog`（应拆 dialogs）
- 文件里定义了 `class XxxButton : QWidget`（应拆 widgets）
- 文件里长段 `setStyleSheet(R"(...)" )`（应抽到 theme/qss 或 ThemeManager）
- slot 函数里又写了一堆 UI 创建代码（应拆成 widget 或 helper）

## 4. 拆分执行步骤（最不容易炸的顺序）

- 先抽纯函数（零依赖）：format / parse / convert
- 再抽独立组件：Card/Button/NavItem（只发信号）
- 再抽 Dialog：FormDialog/ConfigDialog
- 再抽 Model/Delegate（如果是表格/列表）
- 最后让 Page/View 只剩“组装 + 连接信号 + 调用 service”

## 5. Include / 编译依赖 checklist（避免循环引用）

- `.h` 里尽量用 forward declaration（前置声明）
- 只在 `.cpp` include 重头文件（Service/Theme/Widget）
- UI 组件头文件不要 include Service 头文件
- 模块之间的依赖方向固定：
  - `views → widgets/dialogs/models/services/utils`
  - `widgets/dialogs → utils/theme`（最多）
  - `services/models/utils` 不能依赖 `views/widgets`

## 6. Qt moc / Q_OBJECT 拆分 checklist

- 不要在 `.cpp` 里定义带 `Q_OBJECT` 的类（除非你很清楚 moc include）
- 每个带 `Q_OBJECT` 的类都放到独立 `.h`
- 删除手动 `#include "xxx.moc"`（让 CMake AUTOmoc / qmake 接管）
- 若用 qmake：确保 `.pro` 里 HEADERS/SOURCES 都列了

## 7. 完成后的“健康状态”验收

- Page/View：200–400 行，入口函数明显
- Widget/Component：100–300 行，职责单一
- Dialog：100–300 行，提供 `data()/setData()`
- 工具函数可独立复用、可单测
- 改 UI 不会触发业务层编译大范围重编
