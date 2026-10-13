# 常见问题

## 编译相关

**Q：CMake 报 `Could not find a package configuration file provided by "Qt6"`**
A：`CMAKE_PREFIX_PATH` 指向了 MinGW 版 Qt。必须指向 MSVC 版，例如 `C:/Qt/6.9.3/msvc2022_64`。

**Q：`fatal error C1083: 无法打开包括文件: "uhd.h"`**
A：未安装 UHD SDK。去掉 `HAS_UHD` 编译定义并从 `target_link_libraries` 中删掉 `${UHD_LIBRARY}`。

**Q：源文件含中文导致 `error C2001: 常量中有换行符`**
A：MSVC 默认按 GBK(936) 解析。加 `/utf-8` 编译参数。

**Q：`warning C4819: 该文件包含不能在当前代码页(936)中表示的字符`**
A：同上，加 `/utf-8`。

## 运行相关

**Q：程序启动后弹出黑色 cmd 窗口**
A：目标须为 `WIN32` 且链接 `/SUBSYSTEM:WINDOWS`。CMake 中设置 `WIN32_EXECUTABLE TRUE`。

**Q：地图页面空白**
A：QtWebEngine 未完整部署。需连同 `QtWebEngineProcess.exe`、`resources/`、`translations/` 一起复制，建议用 `windeployqt`。

**Q：找不到 SDR 设备**
A：HackRF 需 `hackrf.dll` 与驱动；PlutoSDR 需 `libiio.dll`；USRP 需 UHD 运行库与正确固件。检查设备是否被其他进程占用。

**Q：四联图一直显示 WAITING**
A：表示未收到 IQ 数据。检查 `gps-core` 是否启动、TCP 1999 是否连通。

**Q：发射功率调不上去**
A：不同设备增益上限不同（HackRF TX VGA 最大约 47dB）。检查天线与功放，注意不要超过法规限制。

## 授权相关

**Q：提示授权无效**
A：确认机器码与授权文件匹配、授权未过期。授权文件由签发工具生成，本仓库不含签发工具（见 [license.md](license.md)）。

**Q：换电脑后授权失效**
A：授权绑定机器码，需重新签发。

## 地图相关

**Q：地图瓦片加载失败 / 空白**
A：百度或天地图的 key 失效或额度用尽。检查 `static/qtmap.html` 中的 key 配置，或运行 `download_tiles.py` 下载离线瓦片。

**Q：坐标偏移**
A：国内地图需坐标转换（WGS84 → GCJ02 → BD09）。`static/coordtransform.min.js` 负责转换，确认地图源与坐标系匹配。

## 热更新相关

**Q：更新失败**
A：查看程序目录下 `updater.log`。常见原因：R2 凭证失效、网络不通、文件被占用。

**Q：更新后程序打不开**
A：MD5 校验失败可能导致文件损坏，删除出问题的文件让更新器重新下载。
