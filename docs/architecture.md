# 架构与数据流

## 总体结构

```
┌─────────────────────────────────────────────────────────────┐
│                    gps-ui-qt.exe (Qt6 GUI)                  │
│                                                             │
│  ┌──────────┐   ┌──────────────┐   ┌────────────────────┐   │
│  │ 控制面板  │   │  四联图显示   │   │  地图页 (WebEngine) │   │
│  │ 启动/停止 │   │ 频谱/瀑布/    │   │  百度/天地图        │   │
│  │ 功率/设备 │   │ 时域/星座     │   │  轨迹/路径规划      │   │
│  └────┬─────┘   └──────▲───────┘   └─────────▲──────────┘   │
│       │                │                     │              │
│       │ cmd            │ IQ 数据             │ JS 回调       │
│       ▼                │                     │              │
│  ┌─────────────────────┴─────────────────────┴──────────┐   │
│  │              UsrpThread (QThread 设备线程)             │   │
│  │         rf_core.c — SDR 设备抽象层                     │   │
│  │    ┌──────────┬──────────┬──────────────┐             │   │
│  │    │  HackRF  │  USRP    │  PlutoSDR    │             │   │
│  │    │ 动态加载  │  UHD     │  AD936x      │             │   │
│  │    └──────────┴──────────┴──────────────┘             │   │
│  └────────────────────────▲──────────────────────────────┘   │
│                           │ TCP :1999                       │
└───────────────────────────┼─────────────────────────────────┘
                            │
                  ┌─────────┴──────────┐
                  │   gps-core.exe     │
                  │  gpssim.c 信号生成  │
                  │  (RINEX→IQ 采样)   │
                  └────────────────────┘
```

## 数据流

1. `gps-core` 读取星历文件（RINEX `*.2Xn` 或广播星历），按时间步生成 GPS L1 C/A 基带 IQ 采样
2. IQ 数据经 **TCP 1999** 端口流式推送给 GUI 的 `rf_core.c`
3. 设备线程 `UsrpThread` 通过回调 `onUsrpIq()` 拿到 IQ，一路写入 SDR 硬件发射，一路分发给 `ChartWidget::setData()`
4. 四联图各自做显示处理（详见 [signal-charts.md](signal-charts.md)）

**IQ 格式**：`[I0, Q0, I1, Q1, ...]`，每个采样点两个 float，约 1000 帧/秒、每帧 2560 采样点。

## 线程模型

| 线程 | 职责 |
|------|------|
| GUI 主线程 | 界面绘制、用户交互、授权校验 |
| `UsrpThread` | SDR 设备收发、IQ 回调分发 |
| WebEngine 渲染进程 | 独立进程运行地图页面 |

跨线程通信用 Qt 信号槽（`QueuedConnection`）；`rf_context.running` 用 `volatile int` 标志控制启停。

## 通信协议（GUI ↔ gps-core）

- 传输：TCP，`localhost:1999`（`socket.c` 同时兼容 Windows `WSAStartup` 与 Linux socket）
- 方向：`gps-core` 为服务端还是客户端由启动参数决定，GUI 端 `socket.c` 负责连接与握手
- 载荷：原始 float 数组（IQ），无压缩

## 地图交互协议

地图页与 C++ 通过 **JS console 消息**通信（`MapPage::javaScriptConsoleMessage`）：

| 前缀 | 方向 | 含义 |
|------|------|------|
| `MODE_SWITCH:` | JS→C++ | 切换模式（1/2/3 → 0/1/2） |
| `SINGLE_PREVIEW:` | JS→C++ | 单点预览坐标 `lon,lat` |
| `ROUTE_TARGET:` | JS→C++ | 路线目标点 |
| `PATH_UPDATE:` | JS→C++ | 路径进度 `当前:总数` |

详见 [map.md](map.md)。

## 相关源码

| 文件 | 说明 |
|------|------|
| `gps-ui-qt/gps-ui-qt/src/core/gpssim.c` | GPS 信号生成核心（源自 gps-sdr-sim） |
| `gps-ui-qt/gps-ui-qt/src/core/rf_core.c` | SDR 设备抽象层 |
| `gps-ui-qt/gps-ui-qt/src/core/socket.c` | TCP 通信 |
| `gps-ui-qt/gps-ui-qt/src/mainwindow.cpp` | 主窗口、四联图、卫星面板 |
| `gps-ui-qt/gps-ui-qt/src/mapwindow.cpp` | 地图窗口 |
