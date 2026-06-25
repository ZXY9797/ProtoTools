# KPtools

KPtools 是一个基于 Qt 6 的桌面协议调试和固件升级工具，面向 KP2 协议、嵌入式设备调试和产测升级场景。当前支持 UART、USB Bulk、CAN/CAN FD 和 BLE 链路，并提供协议监控、协议编辑、快捷指令、协议校验、Lua 脚本、协议曲线和固件升级功能。

![主界面与协议监控](docs/images/main-monitor.png)

## 主要功能

- 链路配置：UART、USB Bulk、ZLG CAN/CAN FD、BLE。
- 协议监控：按时间显示收发帧，支持命令回显、字段过滤、完整 Data 预览和流量统计。
- 协议编辑：手动构造 KP2 协议帧，预览 Hex，发送后显示完整回包。
- 快捷指令：从协议编辑添加，支持卡片内编辑、发送和删除。
- 协议校验：输入二进制 Hex 后反解协议内容，错误时提示原因。
- 协议曲线：从 Data 偏移解析数值，最多显示九条曲线，支持隐藏曲线和时间缩放。
- Lua 脚本：可过滤监控数据、修改显示颜色、自动回复、解析 Data 并推送曲线值。
- 固件升级：支持查询版本、单次升级、升级压测、升级进度和成功次数显示。
- 打包发布：提供 Windows 一键打包脚本，生成可双击运行的 `dist/KPtools.exe`。

## 界面预览

![USB 链路与升级面板](docs/images/usb-link-upgrade.png)

![CAN 固件升级](docs/images/firmware-upgrade-can.png)

![UART 与协议编辑](docs/images/protocol-editor-uart.png)

## 目录结构

```text
.
├── app/                         # Qt 应用源码、QML、资源和工具脚本
│   ├── qml/                     # 主界面和各功能页面
│   ├── rcc/                     # 应用图标、Windows 资源文件
│   ├── src/                     # C++ 业务代码、链路驱动、协议和数据模型
│   └── tools/zlg_can_bridge.py  # ZLG CAN/CAN FD Python 桥接进程
├── config/                      # 默认脚本和运行时配置目录
├── docs/images/                 # README 截图
├── lib/                         # 第三方源码：QCodeEditor、Lua
├── scripts/                     # 构建和打包脚本
└── CMakeLists.txt
```

`build-*`、`dist/`、Qt Creator 用户配置和运行时 `config/settings.ini` 都不会提交到仓库。

## 最小工具链

Windows 构建推荐环境：

- Windows x64。
- Visual Studio 2022 Build Tools，安装 MSVC x64 工具链。
- CMake 3.20 或更高版本。
- Ninja。
- Qt 6 MSVC x64，当前验证路径为 `D:\software\Qt6.11\6.11.1\msvc2022_64`。
- Python 3，用于单 exe 打包脚本安装并运行 PyInstaller。

链路相关依赖：

- USB：Windows 下设备需要绑定 WinUSB；Linux 下使用 libusb。
- CAN/CAN FD：使用 ZLG CAN 设备时需要 `zlgcan.dll` 和对应硬件驱动。
- BLE：需要主机蓝牙适配器和系统 BLE 支持。

## 源码编译

在仓库根目录执行：

```powershell
$env:QtRoot = "D:\software\Qt6.11\6.11.1\msvc2022_64"
cmd.exe /d /s /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"" -arch=x64 -host_arch=x64 && cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=""%QtRoot%"" && cmake --build build-release --config Release --target KPtools"
```

构建产物位于：

```text
build-release/app/KPtools.exe
```

这个 exe 是普通 Qt 构建产物，调试时可直接使用；发布给其他电脑使用时建议执行下面的打包脚本。

## 一键打包单文件 exe

默认打包完整功能，包括 Qt 运行库、MSVC 运行库、ZLG CAN 桥接程序和可选的 `zlgcan.dll`：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package-windows-single-exe.ps1 -RunLaunchTest
```

如果脚本找不到 `zlgcan.dll`，可以手动指定：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package-windows-single-exe.ps1 -ZlgDll "D:\path\to\zlgcan.dll" -RunLaunchTest
```

如果只需要不含 ZLG CAN 的包，可以使用：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package-windows-single-exe.ps1 -SkipZlgCan -RunLaunchTest
```

脚本会执行以下步骤：

1. 使用 MSVC + Qt 配置并构建 Release。
2. 将 `app/tools/zlg_can_bridge.py` 打包成 `zlg_can_bridge.exe`。
3. 使用 `windeployqt` 收集 Qt 运行库和 QML 插件。
4. 复制 MSVC 运行库和可选的 ZLG CAN DLL。
5. 使用 PyInstaller 封装成单文件 `KPtools.exe`。
6. 删除临时构建和部署目录，只保留最终 exe。

最终产物：

```text
dist/KPtools.exe
```

说明：单 exe 会携带应用 DLL 依赖，但不能携带硬件内核驱动。目标电脑仍需要具备对应 USB/CAN/BLE 设备驱动能力。

## 常用操作

1. 打开 `KPtools.exe`。
2. 在右侧链路配置中选择 `UART`、`USB`、`CAN` 或 `BLE`。
3. 填写链路参数并点击 `打开连接`。
4. 默认进入固件升级页面，可先点击 `查询版本` 验证链路。
5. 使用 `单次升级` 执行一次升级，或设置 `压测次数` 后执行升级压测。
6. 需要手动发协议时，切换到 `协议编辑` 或 `快捷指令`。
7. 左侧可在 `协议监控`、`终端`、`协议曲线` 和 Lua 脚本窗口之间切换。

## 开发备注

- KP2 协议的构造和解析集中在 `app/src/DataModel/ProtocolEngine.*`。
- 固件升级流程集中在 `app/src/DataModel/FirmwareUpgradeManager.*`。
- 链路生命周期和发送接收入口集中在 `app/src/IO/ConnectionManager.*`。
- USB Bulk 驱动在 `app/src/IO/Drivers/UsbBulk.*`。
- ZLG CAN/CAN FD 驱动在 `app/src/IO/Drivers/ZlgCanFd.*`，运行时通过 `zlg_can_bridge.exe` 调用 ZLG DLL。
- QML UI 主要位于 `app/qml/Panes` 和 `app/qml/Widgets`。
- 第三方源码只保留当前应用需要的库源码，示例、离线文档和归档包不再随仓库提交。
