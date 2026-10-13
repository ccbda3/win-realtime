# gps-ui-qt — 主程序

GPS 实时发射软件的主工程，包含 Qt6 图形界面版与命令行版。

## 内容

```
gps-ui-qt/
├── CMakeLists.txt          顶层构建脚本（版本变体开关）
├── appicon.rc              程序图标资源
├── favicon.ico             图标
├── 卫星.png                卫星状态面板素材
└── gps-ui-qt/              工程源目录
    ├── CMakeLists.txt      子目录构建脚本
    ├── src/                源码
    └── static/             地图前端页面
```

## 构建目标

| 目标 | 产物 | 说明 |
|------|------|------|
| `gps-ui-qt` | `gps-ui-qt.exe` | Qt6 图形界面主程序 |
| `gps-core` | `gps-core.exe` | 命令行信号发生器 |

## 版本变体

顶层 `CMakeLists.txt` 通过 `VERSION_TYPE` 切换：

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64 -DVERSION_TYPE=gaoduan gps-ui-qt
```

| 值 | 产品 | 输出目录 |
|----|------|----------|
| `quanneng`（默认） | 全能版 | `gps-ui-qt/gps-ui-qt_portable` |
| `gaoduan` | 高端版 | `gaoduan/gps-ui-qt_portable` |

## 依赖

Qt6 `Core` `Widgets` `Network` `WebEngineWidgets` `Concurrent`，UHD SDK（可选）。

详见 [../docs/build.md](../docs/build.md)。
