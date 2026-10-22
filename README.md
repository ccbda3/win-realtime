<p align="center">
  <img src="docs/images/logo.png" width="140" alt="行迹自录 · GPS 卫星">
</p>

<h1 align="center">行迹自录 · GPS 实时发射软件</h1>

<p align="center">
  基于 <a href="https://github.com/osqzss/gps-sdr-sim">gps-sdr-sim</a> 改造的 Windows GPS L1 C/A 实时信号仿真与发射软件
</p>

<p align="center">
  <img src="https://img.shields.io/badge/platform-Windows%2010%2F11-blue" alt="platform">
  <img src="https://img.shields.io/badge/Qt-6.9-green" alt="Qt">
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C" alt="C++17">
  <img src="https://img.shields.io/badge/license-MIT-orange" alt="license">
</p>

---

## 简介

通过 SDR 硬件实时生成并发射 GPS L1 C/A 信号，配套地图轨迹规划、信号频谱分析与卫星状态监控。与离线采样回放方案不同，本软件采用**实时流式发射**：星历经 `gps-core` 实时生成基带 IQ，经 TCP 送入 GUI 设备线程，边生成边发射。

## 功能特性

### 信号仿真
- GPS L1 C/A 码信号生成（支持 RINEX 星历 / 广播星历）
- 实时流式发射，非离线回放
- 可调发射功率等级

### SDR 设备
- **HackRF** — 动态加载 `hackrf.dll`，环形缓冲 TX
- **USRP / UHD** 系列
- **PlutoSDR** — AD936x

### 信号分析（四联图）
数据源统一为 IQ 采样流：

| 图表 | 内容 |
|------|------|
| 频谱图 | FFT 频域幅值 (dB) + maxHold 轨迹，滚轮缩放 / 拖拽平移 |
| 瀑布图 | 256 色索引图逐帧上滚，蓝→青→黄→白色板 |
| 时域图 | 原始 IQ 波形，中心零点 |
| 星座图 | I/Q 散点 + 同心圆网格 |

### 卫星状态面板
33 个 PRN 网格实时显示、信号强度指示、链路状态。

### 地图与路径
- **百度地图 / 天地图** 双源切换（Leaflet + WebEngine）
- 轨迹录制与回放、路线规划、目标点选取
- WGS84 / GCJ02 / BD09 坐标转换

### 工程能力
- RSA-2048 授权码验证（客户端仅内嵌公钥）
- Cloudflare R2 增量热更新
- Inno Setup 多版本安装包（全能版 / 高端版 / 综合版）
- 树莓派端 BLE 星历传输

## 文档导航

| 文档 | 内容 |
|------|------|
| [docs/architecture.md](docs/architecture.md) | 总体架构、数据流、线程模型、通信协议 |
| [docs/build.md](docs/build.md) | 环境要求、编译、版本变体、打包 |
| [docs/devices.md](docs/devices.md) | SDR 设备抽象层与各设备支持 |
| [docs/signal-charts.md](docs/signal-charts.md) | 四联图绘制参数详解 |
| [docs/map.md](docs/map.md) | 地图模块开发文档（天地图 / 百度） |
| [docs/ble.md](docs/ble.md) | 蓝牙星历传输 |
| [docs/hotupdate.md](docs/hotupdate.md) | 热更新原理与发布流程 |
| [docs/license.md](docs/license.md) | 授权机制 |
| [docs/faq.md](docs/faq.md) | 常见问题排查 |

## 目录结构

> 项目名称：**行迹自录**；仓库名与本地目录名保持 `win-realtime`。

```
win-realtime/
├── docs/                       文档（含架构、构建、设备、地图、FAQ）
│   └── images/                 文档图片
├── gps-ui-qt/                  主程序工程
│   ├── README.md
│   └── gps-ui-qt/
│       ├── README.md
│       ├── src/                源码 → src/README.md
│       │   ├── main.cpp            入口、授权校验
│       │   ├── mainwindow.cpp/h    主窗口、四联图、卫星面板
│       │   ├── mapwindow.cpp/h     地图窗口
│       │   ├── rsa_verify.c/h      授权验签（仅公钥）
│       │   └── core/               核心模块 → 见 src/README.md
│       │       ├── gpssim.c/h          GPS 信号生成
│       │       ├── rf_core.c           SDR 设备抽象
│       │       └── socket.c            TCP 通信
│       └── static/             地图前端 → static/README.md
├── hotupdate/                  热更新组件 → hotupdate/README.md
├── BLEManager/                 蓝牙星历传输 → BLEManager/README.md
│   └── blec/                   树莓派 btstack 守护进程
├── env_check/                  环境检测工具 → env_check/README.md
├── stage/                      打包清单与版本模板 → stage/README.md
├── installer*.iss              Inno Setup 安装脚本
├── download_tiles.py           离线地图瓦片下载
└── CONTRIBUTING.md             贡献指南
```

## 快速开始

```bash
# 1. 按本机路径修改 gps-ui-qt/CMakeLists.txt 中的 CMAKE_PREFIX_PATH 与 UHD_DIR
# 2. 配置构建
cmake -B build -G "Visual Studio 17 2022" -A x64 gps-ui-qt
# 3. 编译
cmake --build build --config Release
# 4. 部署 Qt 运行库
windeployqt --release --no-opengl-sw <输出目录>/gps-ui-qt.exe
```

产物：`gps-ui-qt.exe`（图形界面）、`gps-core.exe`（命令行）。

完整步骤见 [docs/build.md](docs/build.md)。

## 数据流

```
星历 → gps-core.exe → TCP:1999 → rf_core.c (设备线程)
                                    ↓ onUsrpIq()
                              ChartWidget::setData()
                                    ↓
                      频谱 / 瀑布 / 时域 / 星座 四联图
```

IQ 格式 `[I0, Q0, I1, Q1, ...]`，每点两个 float，约 1000 帧/秒、每帧 2560 采样点。

详见 [docs/architecture.md](docs/architecture.md)。

## 关于授权

客户端 `rsa_verify.c` 内嵌 **RSA-2048 公钥**，验证授权文件签名；授权文件格式为 `机器码|类型|时间戳` + PKCS#1 v1.5 SHA-256 签名。

**签发工具（含私钥）不在本仓库中**，详见 [docs/license.md](docs/license.md)。

## 免责声明

- 本项目仅供 GPS 信号仿真研究、教学与授权测试使用
- 请遵守当地无线电法规，在屏蔽环境或合法频段下使用
- 星历文件（`*.2Xn`）需自行从 [IGS](https://igs.org/) 或广播星历获取

## 许可证

[MIT License](LICENSE)

## 致谢

- [gps-sdr-sim](https://github.com/osqzss/gps-sdr-sim) — GPS 信号仿真核心
- [Leaflet](https://leafletjs.com/) — 地图渲染
- [btstack](https://github.com/bluekitchen/btstack) — 树莓派 BLE 协议栈
- 百度地图 / 天地图 — 地图数据服务
