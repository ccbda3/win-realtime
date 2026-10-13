# 构建指南

## 环境要求

| 依赖 | 版本 | 说明 |
|------|------|------|
| Windows | 10 / 11 x64 | — |
| Visual Studio | 2022 或更高 | MSVC 工具链 |
| Qt | 6.9.3 (msvc2022_64) | 需 `Widgets` `Network` `WebEngineWidgets` `Concurrent` |
| UHD SDK | 4.x（可选） | USRP 支持，默认 `C:/Program Files/UHD` |
| HackRF | 运行时动态加载 | 编译无需，运行时需 `hackrf.dll` |
| PlutoSDR | libiio | 运行时需 `libiio.dll` |
| CMake | 3.20+ | — |
| Inno Setup | 6.x（打包用） | — |

## 路径配置

编译前修改 `gps-ui-qt/CMakeLists.txt` 顶部的本机路径：

```cmake
set(CMAKE_PREFIX_PATH "C:/Qt/6.9.3/msvc2022_64")   # 改成你的 Qt 路径
set(UHD_DIR "C:/Program Files/UHD")                # 无 USRP 可留空并去掉 HAS_UHD
```

输出目录 `PORTABLE_DIR` 也写死在文件里，建议改成本机路径。

## 编译主程序

```bash
cd win-realtime
cmake -B build -G "Visual Studio 17 2022" -A x64 gps-ui-qt
cmake --build build --config Release
```

产物：

| 文件 | 说明 |
|------|------|
| `gps-ui-qt.exe` | 图形界面主程序 |
| `gps-core.exe` | 命令行信号发生器 |

编译后会自动把 `gps-ui-qt/static/` 复制到输出目录。

## 运行时依赖（windeployqt）

```bash
windeployqt --release --no-translations --no-opengl-sw <输出目录>/gps-ui-qt.exe
```

Qt WebEngine 需要完整部署，包括 `QtWebEngineProcess.exe` 及 `resources/`、`translations/` 目录。

## 版本变体

通过 CMake 变量切换版本：

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64 -DVERSION_TYPE=gaoduan gps-ui-qt
```

| VERSION_TYPE | 产品名 | 输出目录 |
|--------------|--------|----------|
| `quanneng`（默认） | 行迹自录-全能版 | `gps-ui-qt/gps-ui-qt_portable` |
| `gaoduan` | 行迹自录-高端版 | `gaoduan/gps-ui-qt_portable` |

`gaoduan` 会额外定义 `GPS_VERSION_GAODUAN` 宏。

## 热更新组件

`hotupdate/` 单独编译，需要先准备 R2 凭证：

```bash
cp hotupdate/r2_credentials.h.example hotupdate/r2_credentials.h
# 编辑 r2_credentials.h 填入自己的 Cloudflare R2 凭证
```

| 文件 | 产物 | 说明 |
|------|------|------|
| `updater.c` | `updater.exe` | 主更新器，读 version.json 比对 MD5 拉取 |
| `push_update.c` | 上传工具 | 把构建产物推到 R2 |
| `toggle_render.c` | `切换CPU渲染.exe` / `切换GPU渲染.exe` | 切换 QtWebEngine 渲染后端 |

## 打包安装程序

```bash
# 设置版本号环境变量（可选，默认 1.0.0）
set GIT_VERSION=1.0.13
iscc installer.iss          # 全能版
iscc installer_gaoduan.iss  # 高端版
iscc installer_zonghe.iss   # 综合版
```

输出到 `installer_output/`。

## 常见问题

**编译报找不到 Qt6**
检查 `CMAKE_PREFIX_PATH` 是否指向 `.../msvc2022_64`（不是 `mingw_64`）。

**找不到 uhd.lib**
未装 UHD SDK 时，把 `HAS_UHD` 编译定义去掉，并从链接库中移除 `${UHD_LIBRARY}`。

**运行时缺 DLL**
用 `windeployqt` 部署；WebEngine 相关文件较多，务必整目录复制。

**黑框 cmd 窗口**
所有 exe 都用 `/SUBSYSTEM:WINDOWS` + `WIN32` 目标属性，若自行新增可执行文件需一并设置。
