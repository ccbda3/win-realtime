# src — 源码目录

## 文件清单

| 文件 | 说明 |
|------|------|
| `main.cpp` | 程序入口、授权码校验、全局状态接口、WebEngine 初始化 |
| `mainwindow.cpp/.h` | 主窗口：控制面板、四联图、卫星状态面板、设备线程 |
| `mapwindow.cpp/.h` | 地图窗口（WebEngine 承载） |
| `rsa_verify.c/.h` | RSA-2048 授权码验签（仅含公钥，Windows CNG） |
| `getopt.c/.h` | 命令行参数解析（跨平台，供 gps-core 使用） |
| `gpssim.c/.h` | GPS 信号生成（副本，核心在 `core/`） |
| `rf_core.c` | SDR 设备抽象（副本，核心在 `core/`） |
| `socket.c` | TCP 通信（副本，核心在 `core/`） |

> `src/` 下与 `core/` 同名的文件为历史遗留副本，**实际参与编译的是 `core/` 下的版本**（见 CMakeLists）。

## core/ — 核心模块

| 文件 | 说明 |
|------|------|
| `core/gpssim.c/.h` | GPS L1 C/A 信号生成核心（源自 gps-sdr-sim） |
| `core/rf_core.c` | SDR 设备抽象层：HackRF / USRP / PlutoSDR |
| `core/socket.c` | TCP 通信层，Windows 与 Linux 兼容 |

## 关键类

| 类 | 职责 |
|----|------|
| `MainWindow` | 主窗口，管理设备线程与各面板 |
| `UsrpThread` | SDR 设备线程，`rf_run()` 宿主，回调分发 IQ |
| `ChartWidget` | 四联图绘制（频谱/瀑布/时域/星座） |
| `SatelliteStatusWidget` | 33 PRN 卫星状态面板 |
| `ToggleSwitch` | 自定义开关控件 |
| `MapPage` | `QWebEnginePage` 子类，拦截 JS console 消息做双向通信 |

## 数据流

`gps-core.exe` → TCP:1999 → `rf_core.c` → `onUsrpIq()` → `ChartWidget::setData()`

详见 [../../docs/architecture.md](../../docs/architecture.md)。
