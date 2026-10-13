# static — 地图前端页面

Qt WebEngine 加载的地图页面，与 C++ 侧通过 JS console 消息通信。

## 文件

| 文件 | 说明 |
|------|------|
| `qtmap.html` | 地图主页面（约 98KB），含百度 / 天地图两套初始化逻辑 |
| `index.js` | 地图逻辑：模式切换、坐标转换、搜索、路线规划 |
| `leaflet.js` / `leaflet.css` | Leaflet 地图库（本地，避免外网依赖） |
| `jquery.js` | jQuery |
| `coordtransform.min.js` | WGS84 / GCJ02 / BD09 坐标互转 |

## 地图源

| 源 | 说明 |
|----|------|
| **百度地图** | 通过 `callback=initMapApi` 加载，BD09 坐标 |
| **天地图** | T API v4.0，通过 `onload=initMapApi` 加载，WGS84 坐标 |

切换入口：`qtSwitchMapSource(source, key)`。

## 页面占位符

C++ 在加载前替换 HTML 中的占位符：

| 占位符 | 含义 |
|--------|------|
| `__MAP_SOURCE__` | 地图源（`baidu` / `tianditu`） |

## 与 C++ 通信

页面通过 `console.log()` 发送消息，`MapPage::javaScriptConsoleMessage` 拦截解析：

| 前缀 | 示例 | 含义 |
|------|------|------|
| `MODE_SWITCH:` | `MODE_SWITCH:2` | 切换模式 |
| `SINGLE_PREVIEW:` | `SINGLE_PREVIEW:116.3,39.9` | 单点预览 |
| `ROUTE_TARGET:` | `ROUTE_TARGET:116.3,39.9` | 路线目标 |
| `PATH_UPDATE:` | `PATH_UPDATE:12:50` | 路径进度 |

## 离线瓦片

`download_tiles.py`（仓库根目录）可批量下载瓦片到本地目录，用于无网环境。

详细开发文档见 [../../docs/map.md](../../docs/map.md)。
