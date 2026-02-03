<p align="center">
  <img src="resources/icons/app.png" alt="Sing-Box Qt" width="128" height="128">
</p>

<h1 align="center">Sing-Box Qt</h1>

<p align="center">
  <strong>基于 Qt6 的 sing-box 图形化客户端</strong>
</p>

<p align="center">
  <a href="#功能特性">功能特性</a> •
  <a href="#截图预览">截图预览</a> •
  <a href="#下载安装">下载安装</a> •
  <a href="#编译构建">编译构建</a> •
  <a href="#技术架构">技术架构</a> •
  <a href="#许可证">许可证</a>
</p>

---

## 功能特性

### 核心功能

- **内核管理** - 自动下载、启动和管理 sing-box 内核进程
- **订阅管理** - 支持多种订阅格式（Base64、YAML、JSON）的导入与更新
- **代理切换** - 一键切换节点，支持延迟测试和自动选择
- **系统代理** - 自动配置 Windows 系统代理设置
- **TUN 模式** - 支持透明代理（需管理员权限）

### 订阅支持

| 格式 | 支持状态 |
|------|----------|
| Sing-box JSON | 原生支持 |
| Clash YAML | 自动转换 |
| Base64 URI | 自动解析 |
| 手动添加节点 | 支持 |

### 协议支持

- **VMess** - V2Ray 协议
- **VLESS** - XRAY 协议
- **Trojan** - Trojan 协议
- **Shadowsocks** - SS 协议
- **Hysteria2** - Hysteria2 协议

### 界面功能

- **仪表盘** - 实时流量监控与统计
- **代理视图** - 节点列表与分组管理
- **连接监控** - 活动连接查看与管理
- **规则查看** - 路由规则可视化
- **日志输出** - 实时日志查看
- **设置选项** - 丰富的配置选项

---

## 截图预览

![主页](https://github.com/user-attachments/assets/84b6067e-eda4-4a70-a2b2-8d0cd574e8d1)



![代理](https://github.com/user-attachments/assets/0ffdde0f-e0c2-4712-a165-e48299c59103)


![订阅](https://github.com/user-attachments/assets/1d70f5b3-0570-41c2-aa11-4526c5c773ab)


![连接](https://github.com/user-attachments/assets/fecc3da5-ec3a-4dc6-8cb4-1945ce277935)


![规则](https://github.com/user-attachments/assets/d07f8b20-2c08-400b-b7ca-6760c3342aeb)


![日志](https://github.com/user-attachments/assets/adcf7a3d-b360-4a7e-baf0-76e17c79d4bc)


![设置](https://github.com/user-attachments/assets/24871ad1-ba63-479e-973d-8066528afd8f)


![托盘](https://github.com/user-attachments/assets/47ea51f1-6a3c-4e15-b5aa-c62d883b1044)



---

## 下载安装

### 便携版（推荐）

1. 下载最新的 `sing-box-qt-portable.zip`
2. 解压到任意目录
3. 运行 `sing-box-qt.exe`

### 系统要求

- **操作系统**: Windows 10/11 (64-bit)
- **运行时**: 无需额外安装（Qt 运行时已内置）

### 首次使用

1. 启动程序后，点击右下角设置图标，点击下载内核
2. 程序会自动下载 sing-box 内核
3. 添加订阅链接或手动添加节点
4. 选择节点并点击启动

---

## 编译构建

### 环境要求

| 依赖 | 版本 | 说明 |
|------|------|------|
| Qt | 6.5+ | 推荐 6.10.x |
| CMake | 3.20+ | 构建工具 |
| MSVC | 2022 | 编译器 |
| Visual Studio | 2022 | 可选 IDE |

### 第三方库（自动获取）

- [nlohmann/json](https://github.com/nlohmann/json) v3.11.3 - JSON 解析
- [yaml-cpp](https://github.com/jbeder/yaml-cpp) v0.8.0 - YAML 解析

### 构建步骤

#### 方式一：使用脚本（推荐）

```powershell
# 修改 build.ps1 中的 Qt 路径
# $QtPath = "C:\Qt\6.10.1\msvc2022_64"

# 执行构建
.\build.ps1
```

#### 方式二：手动构建

```powershell
# 配置项目
cmake -S . -B build -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="C:\Qt\6.10.1\msvc2022_64"

# 编译 Release 版本
cmake --build build --config Release

# 可执行文件位于
# .\build\Release\sing-box-qt.exe
```

### 打包分发

```powershell
# 构建并打包便携版
.\pack.ps1

# 生成文件：sing-box-qt-portable.zip
```

---

## 技术架构

```
sing-box-qt/
├── src/
│   ├── main.cpp              # 程序入口
│   ├── app/                  # 应用层
│   │   ├── MainWindow        # 主窗口
│   │   └── TrayIcon          # 系统托盘
│   ├── core/                 # 核心逻辑
│   │   ├── KernelService     # 内核服务
│   │   ├── ProcessManager    # 进程管理
│   │   └── ProxyController   # 代理控制
│   ├── dialogs/              # 对话框
│   │   ├── config/           # 配置对话框
│   │   ├── editors/          # 编辑对话框
│   │   ├── rules/            # 规则对话框
│   │   └── subscription/     # 订阅对话框
│   ├── models/               # 数据模型
│   │   └── SettingsModel     # 设置模型
│   ├── network/              # 网络模块
│   │   ├── HttpClient        # HTTP 客户端
│   │   ├── WebSocketClient   # WebSocket 客户端
│   │   └── SubscriptionService # 订阅服务
│   ├── services/             # 业务服务
│   │   ├── config/           # 配置服务
│   │   ├── kernel/           # 内核服务实现
│   │   ├── rules/            # 规则服务
│   │   ├── settings/         # 设置服务
│   │   └── subscription/     # 订阅服务实现
│   ├── storage/              # 存储模块
│   │   ├── DatabaseService   # 数据库服务
│   │   └── AppSettings       # 应用设置
│   ├── system/               # 系统集成
│   │   ├── SystemProxy       # 系统代理
│   │   └── AutoStart         # 开机自启
│   ├── utils/                # 工具库
│   │   ├── Logger            # 日志管理
│   │   ├── Crypto            # 加密工具
│   │   └── ThemeManager      # 主题管理
│   ├── views/                # 视图层
│   │   ├── home/             # 主页视图
│   │   ├── proxy/            # 代理视图
│   │   ├── subscription/     # 订阅视图
│   │   └── settings/         # 设置视图
│   └── widgets/              # 通用组件
│       ├── common/           # 公共组件
│       └── logs/             # 日志组件
└── resources/                # 资源文件
    ├── icons/                # 图标资源
    └── translations/         # 翻译文件
```

### 核心技术栈

- **GUI 框架**: Qt 6 (Widgets)
- **编程语言**: C++17
- **网络通信**: Qt Network + WebSockets
- **数据存储**: SQLite (Qt SQL)
- **配置格式**: JSON / YAML
- **构建系统**: CMake

---

## 相关项目

- [sing-box](https://github.com/SagerNet/sing-box) - 通用代理平台内核
- [sing-box-windows](https://github.com/xinggaoya/sing-box-windows) - Tauri + Vue 版客户端

---

## 许可证

本项目采用 [MIT](LICENSE) 许可证开源。

---

## 致谢

- [SagerNet/sing-box](https://github.com/SagerNet/sing-box) - 强大的代理内核
- [Qt Project](https://www.qt.io/) - 跨平台 GUI 框架
- [nlohmann/json](https://github.com/nlohmann/json) - 现代 C++ JSON 库
- [yaml-cpp](https://github.com/jbeder/yaml-cpp) - C++ YAML 解析库
