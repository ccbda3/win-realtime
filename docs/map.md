# 开发者运维文档 — 天地图算法实现

> **版本**: 1.0  
> **适用项目**: gps-ui-qt (全能/高端/简单版)  
> **最后更新**: 2026-06-28  

---

## 目录

1. [初始化流程](#1-初始化流程)
2. [坐标系统](#2-坐标系统)
3. [瓦片加载](#3-瓦片加载)
4. [搜索系统](#4-搜索系统)
5. [路线规划](#5-路线规划)
6. [地图切换](#6-地图切换)
7. [API 端点汇总](#7-api-端点汇总)
8. [全局变量清单](#8-全局变量清单)
9. [常见问题](#9-常见问题)

---

## 1. 初始化流程

### 1.1 入口与触发条件

天地图的初始化入口在 `qtmap.html` 的 `initMapApi(externalApiKey)` 函数（约第 330 行）。当 `mapSource === "tianditu"` 时执行天地图分支。

触发初始化的三种场景：

| 场景 | 触发方式 | 说明 |
|------|---------|------|
| 页面首次加载 | `mapSource` 由 C++ 替换 `__MAP_SOURCE__` 占位符 | 自动调用 `initMapApi()` |
| C++ 端请求切换 | `qtSwitchMapSource("tianditu", key)` | 内部重建容器后调用 `initMapApi(key)` |
| 百度→天地图切换 | `qtSwitchMapSource` 中的 `doInit()` | 加载 Leaflet/T 库后调用 |

### 1.2 库加载顺序

```
页面加载
  ├── leaflet.js (本地 /static/leaflet.js)
  ├── jquery.js (本地 /static/jquery.js)
  ├── coordtransform.min.js (本地 /static/coordtransform.min.js)
  └── index.js (本地 /static/index.js)
        └── 检查 mapSource
              ├── "baidu" → 加载百度 API (带 callback=initMapApi)
              └── "tianditu" → 加载天地图 T API v4.0 (onload=initMapApi)
```

**关键**: 天地图 T API (`https://api.tianditu.gov.cn/api?v=4.0&tk=...`) 加载完成后通过 `onload` 事件触发 `initMapApi()`，与百度 API 的 `callback` 机制不同。

### 1.3 L.map 创建

```javascript
mapApi.map = L.map("map", { zoomControl: false }).setView([INIT_LAT, INIT_LON], 15);
```

参数：
- `"map"`: DOM 容器 ID
- `zoomControl: false`: 不显示默认缩放控件（避免 UI 遮挡）
- `[INIT_LAT, INIT_LON]`: 初始中心坐标（WGS84）
- `15`: 初始缩放级别

初始化后执行：
```javascript
mapApi.map.doubleClickZoom.disable();  // 禁用双击缩放，避免干扰路径标记
```

### 1.4 瓦片层加载

创建两个瓦片层并添加到 `mapApi.map`：

```javascript
// 矢量基底
L.tileLayer("https://t0.tianditu.gov.cn/DataServer?T=vec_w&x={x}&y={y}&l={z}&tk=" + apiKey, 
  { maxZoom: 18, attribution: "天地图" })

// 矢量注记  
L.tileLayer("https://t0.tianditu.gov.cn/DataServer?T=cva_w&x={x}&y={y}&l={z}&tk=" + apiKey,
  { maxZoom: 18 })
```

两个瓦片层的引用保存在 `window._tiandituLayers` 数组中，以便后续地图类型切换时移除。

### 1.5 隐藏 T.Map 服务实例

```javascript
window._tdtServiceMap = new T.Map("tdtContainer", {
  mapType: T.DEFAULT_MAP,
  center: new T.LngLat(116.39, 39.91),
  zoom: 12
});
```

- 容器 `#tdtContainer` 是一个隐藏的 `<div>`（`position:absolute;left:-9999px;top:-9999px`）
- 该实例仅用于路线规划 (`T.DrivingRoute`) 和搜索 (`T.LocalSearch`)
- 如果初始化失败，`window._tdtServiceMap = null`，后续功能回退到 REST API
- 地图切换时会销毁该实例并重建容器 DOM（避免重复初始化错误）

---

## 2. 坐标系统

### 2.1 天地图坐标体系

天地图使用 **CGCS2000 坐标系**（与中国国家大地坐标系 2000 一致），Web 瓦片 API 提供两种投影：

| 后缀 | 坐标系 | 备注 |
|------|--------|------|
| `_w` | WGS84 (EPSG:4326) | **本项目使用** |
| `_c` | CGCS2000 墨卡托 (EPSG:3857) | 未使用 |

本项目统一使用 **`_w` 后缀的 WGS84 瓦片**。

### 2.2 wgs84ToDisplay（恒等映射）

```javascript
mapApi.wgs84ToDisplay = function(lon, lat) {
  return { lng: lon, lat: lat };
};
```

**输入**: WGS84 经纬度 `(lon, lat)`  
**输出**: Leaflet 显示坐标 `{lng, lat}`（值不变）  
**说明**: 由于使用 WGS84 瓦片，无需任何坐标转换。

### 2.3 displayToWgs84（恒等映射）

```javascript
mapApi.displayToWgs84 = function(lng, lat) {
  return { lon: lng, lat: lat };
};
```

**输入**: Leaflet 事件坐标 `(lng, lat)`  
**输出**: WGS84 经纬度 `{lon, lat}`（值不变）

### 2.4 坐标流程图

```
┌─────────────────────────────────────────────────────────────┐
│                     天地图坐标流                             │
│                                                             │
│  C++后端/UDP数据 (WGS84)                                    │
│       │                                                     │
│       ▼                                                     │
│  window.lon / window.lat (WGS84)                            │
│       │                                                     │
│       ▼                                                     │
│  mapApi.wgs84ToDisplay(lon, lat)                            │
│       │   └→ 恒等映射: {lng: lon, lat: lat}                │
│       ▼                                                     │
│  Leaflet 显示 (L.marker, L.polyline)                        │
│       │                                                     │
│       ▼                                                     │
│  地图点击 e.latlng (lng, lat) = WGS84                       │
│       │                                                     │
│       ▼                                                     │
│  mapApi.displayToWgs84(lng, lat)                            │
│       │   └→ 恒等映射: {lon: lng, lat: lat}                │
│       ▼                                                     │
│  window._previewLon / _previewLat (WGS84)                   │
└─────────────────────────────────────────────────────────────┘
```

### 2.5 与百度地图坐标系统对比

| 项目 | 天地图 | 百度地图 |
|------|--------|---------|
| 瓦片坐标系 | WGS84 (`vec_w`) | BD-09 |
| wgs84ToDisplay | 恒等映射 | WGS84 → GCJ-02 → BD-09 |
| displayToWgs84 | 恒等映射 | BD-09 → GCJ-02 → WGS84 |
| Marker 坐标 | WGS84 (纬度在前) | BD-09 (BMap.Point) |
| 路线 API 返回 | 原生: GCJ-02 / REST: WGS84 | BD-09 |
| 搜索 API 返回 | WGS84 (REST API) | BD-09 |

---

## 3. 瓦片加载

### 3.1 默认瓦片

| 类型 | 参数 | 瓦片 URL | 说明 |
|------|------|---------|------|
| 矢量底图 | `vec_w` | `https://t0.tianditu.gov.cn/DataServer?T=vec_w&x={x}&y={y}&l={z}&tk={key}` | 道路、建筑等矢量地图 |
| 矢量注记 | `cva_w` | `https://t0.tianditu.gov.cn/DataServer?T=cva_w&x={x}&y={y}&l={z}&tk={key}` | 地名、POI 标注 |

### 3.2 地图类型切换 (setMapType)

`mapApi.setMapType(type)` 动态切换瓦片层：

1. 遍历 `window._tiandituLayers`，逐个 `mapApi.map.removeLayer(l)`
2. 清空 `window._tiandituLayers = []`
3. 根据 `type` 添加新的瓦片层：

| type | 基底瓦片 | 注记瓦片 | 效果 |
|------|---------|---------|------|
| `'satellite'` | `img_w` | `cia_w` | 卫星影像 + 影像注记 |
| `'hybrid'` | `img_w` | `cia_w` | 同上（satellite/hybrid 共享） |
| 其他 | `vec_w` | `cva_w` | 矢量地图 + 地名标注 |

所有瓦片层均设置 `maxZoom: 18`。

### 3.3 瓦片代理

当 C++ HTTP 服务器设置 `__TILE_PROXY__` 时，天地图瓦片请求可以通过本地代理转发。此功能在代码中通过字符串替换实现，但实际未启用（`__TILE_PROXY__` 替换后未被瓦片 URL 引用）。

---

## 4. 搜索系统

### 4.1 架构设计

**核心决策**: 跳过天地图原生 `T.LocalSearch`，仅使用 REST API。原因：原生 T API 在 Qt WebEngine 中搜索结果不稳定（返回正确个数但无法渲染到 UI）。

### 4.2 搜索函数 tiandituSearch

```javascript
function tiandituSearch(keyword)
```

#### 请求参数

```json
{
  "keyWord": "搜索关键词",
  "mapBound": "73.0,3.0,135.0,54.0",   // 中国边界，硬编码
  "level": 12,
  "queryType": 1,    // 1=模糊查询
  "start": 0,
  "count": 20        // 最大返回条数
}
```

#### 请求 URL

```
GET https://api.tianditu.gov.cn/v2/search
  ?postStr=<encodeURIComponent(JSON.stringify(params))>
  &type=query
  &tk=<API_KEY>
```

使用 `$.getJSON` 发起 JSONP 请求。

#### 响应解析

```javascript
// 验证成功
data.status.infocode === 1000

// 提取 POI
data.pois.forEach(function(p) {
  var ll = p.lonlat.split(',');  // "lng,lat"
  items.push({
    name: p.name,
    lat: parseFloat(ll[1]),
    lng: parseFloat(ll[0]),
    address: p.address || ''
  });
});

// 行政区兜底（无 POI 时）
if (items.length === 0 && data.area && data.area.lonlat) {
  // 提取行政区坐标作为唯一结果
}
```

### 4.3 结果渲染 showSearchResults

```javascript
function showSearchResults(items)
```

- 清空 `searchResults.innerHTML` 和 `searchData`
- 遍历 `items`，最多显示 20 条
- 每条创建 `<div>` 元素，设置内联样式、hover 高亮、click 事件
- 点击后调用 `flyToPlace(name, lat, lng)`

### 4.4 flyToPlace 飞往地点

```javascript
function flyToPlace(name, lat, lng) {
  window._previewLat = lat;
  window._previewLon = lng;
  var pt = mapApi.wgs84ToDisplay(lng, lat);
  showPreview(pt, lat, lng);
  mapApi.centerAndZoom(pt, 15);
  searchResults.style.display = 'none';
  searchInput.blur();
}
```

- 设置预览坐标
- 通过 `wgs84ToDisplay` 转换（天地图模式下恒等）
- 添加预览标记
- 地图居中到该位置（缩放级别 15）
- 关闭搜索下拉

### 4.5 搜索参数说明

| 参数 | 值 | 说明 |
|------|-----|------|
| `mapBound` | `73.0,3.0,135.0,54.0` | 中国大陆边界，不可用于港澳台 |
| `level` | `12` | 影响搜索结果的排序权重 |
| `queryType` | `1` | 1=模糊匹配，2=精确匹配（未测试） |
| `count` | `20` | 最大 20 条（API 限制） |

### 4.6 搜索 UI 事件

```javascript
searchInput.addEventListener('input', function() {
  var q = this.value.trim();
  if (q.length < 1) { searchResults.style.display = 'none'; return; }
  if (searchTimer) clearTimeout(searchTimer);
  searchTimer = setTimeout(function() {
    if (mapSource === 'tianditu') {
      tiandituSearch(q);
    } else if (mapSource === 'baidu') {
      baiduSearch(q);
    }
  }, 200);  // 200ms 防抖
});
```

---

## 5. 路线规划

### 5.1 总体策略

两级策略：
1. **优先**: 原生 `T.DrivingRoute`（需要 `window._tdtServiceMap` 可用）
2. **回退**: REST drive API（XML 格式）

### 5.2 原生 T API 路线规划

```javascript
var drive = new T.DrivingRoute(window._tdtServiceMap, {
  policy: T.DrivingPolicy.LEAST_TIME,  // 最少时间策略
  onSearchComplete: function(result) {
    if (result.getStatus() === 0) {  // 0 表示成功
      var plan = result.getPlan(0);
      var route = plan.getRoute(0);
      var path = route.getPath();  // T.LngLat 对象数组
      // path[i].lng, path[i].lat → GCJ-02 坐标系
      // 直接存入 {lng, lat}，不转换
    }
  }
});
drive.search(new T.LngLat(fromLon, fromLat), new T.LngLat(toLon, toLat));
```

**注意**: 原生 T API 返回 GCJ-02 坐标，但项目中直接存储为 WGS84（通过恒等 `wgs84ToDisplay` 显示）。这意味着原生 T API 规划的路线在中国境内会有几百米的偏移。这一设计选择与百度地图的实现逻辑一致。

### 5.3 REST API 路线规划

```javascript
var params = {
  orig: fromLon + "," + fromLat,  // 起点 "lng,lat"
  dest: toLon + "," + toLat,      // 终点 "lng,lat"
  style: "0"                       // 0=驾车
};
```

```
GET https://api.tianditu.gov.cn/drive
  ?postStr=<encodeURIComponent(JSON.stringify(params))>
  &type=search
  &tk=<API_KEY>
```

**响应格式**: XML（非 JSON），使用 `$.get` 获取原始文本。

```xml
<result>
  <routelatlon>lng1,lat1;lng2,lat2;lng3,lat3;...</routelatlon>
  ...
</result>
```

解析逻辑：
1. 正则匹配 `<routelatlon>` 标签内容
2. 按 `;` 分割坐标对
3. 按 `,` 分割经纬度
4. 转换为 `{lng, lat}` 对象数组

### 5.4 兜底策略

当 REST API 也失败（无有效路线数据或网络错误），调用 `fallbackStraightLine()`：

```javascript
function fallbackStraightLine(fromLon, fromLat, toLon, toLat) {
  // 在起止点之间画一条橙色虚线 (#ff6600)
  // 并插值中间点
}
```

### 5.5 路线坐标转换 convertRouteToPath

```javascript
function convertRouteToPath(pts, destPoint) {
  // 对于天地图，直接使用 API 返回坐标
  var wgs84 = { lon: pts[i].lng, lat: pts[i].lat };
  
  // 通过 wgs84ToDisplay 转换（天地图恒等）
  var displayPt = mapApi.wgs84ToDisplay(wgs84.lon, wgs84.lat);
}
```

处理流程：
1. 将路线点转换为 `originalPathBD`（显示坐标数组）
2. 计算终点与目标点的距离，如需连接则画橙色虚线
3. 调用 `drawFullPath()` 绘制蓝色路径线
4. 调用 `mapApi.setViewport()` 自适应缩放

### 5.6 路线规划触发方式

| 方式 | 调用方 | 函数 |
|------|--------|------|
| C++ 后端直接调用 | `console.log("PLAN_ROUTE:...")` | `qtPlanRoute(fromLat, fromLon, toLat, toLon)` |
| 地图点击 + 弹窗确认 | `console.log("ROUTE_TARGET:...")` | C++ 收到后调用 `qtPlanRoute` |
| 按地址搜索 | 搜索→点击 | `qtPlanRouteToAddress(addr, fromLat, fromLon)` |

---

## 6. 地图切换

### 6.1 切换流程

`qtSwitchMapSource(newSource, newKey)` 的执行顺序：

```
1. 保存当前状态 (位置、路径、预览、追踪、抖动等)
2. 清除旧路径和路线UI
3. 销毁旧地图实例
   ├── Leaflet: mapApi.map.remove()
   └── BMap: 无标准销毁方法，通过 innerHTML 清空
4. 销毁 T.Map 服务实例并重建容器 DOM
5. 重建 #map DOM 元素 (removeChild + createElement + insertBefore)
6. 设置 mapSource = newSource
7. 检查并加载所需库
   ├── 百度: 动态创建 script 标签 (带 callback)
   └── 天地图: 按需加载 leaflet.js + T API
8. waitForLib → doInit() → initMapApi(newKey)
9. 恢复所有保存的状态
```

### 6.2 库加载判断

| 目标 | 检查条件 | 加载 URL |
|------|---------|---------|
| 百度 | `typeof BMap.Map !== 'function'` | `https://api.map.baidu.com/api?v=2.0&ak={key}&services=true&callback=_baiduSwitchCallback` |
| 天地图 Leaflet | `typeof L.map !== 'function'` | `/static/leaflet.js` |
| 天地图 T API | `typeof T.DrivingRoute !== 'function'` | `https://api.tianditu.gov.cn/api?v=4.0&tk={key}` |

### 6.3 waitForLib 轮询

```javascript
function waitForLib(cb, tries) {
  tries = tries || 0;
  if (tries > 200) { cb(); return; }  // 超时 10 秒后仍执行
  var ready = false;
  if (newSource === "baidu") ready = typeof BMap.Map === 'function';
  else if (newSource === "tianditu") ready = typeof L.map === 'function' && typeof T.DrivingRoute === 'function';
  else ready = true;
  if (ready) { cb(); return; }
  setTimeout(function() { waitForLib(cb, tries + 1); }, 50);
}
```

- 轮询间隔: 50ms
- 最大轮询次数: 200（即最多等待 10 秒）

### 6.4 doInit 执行

```javascript
function doInit() {
  // 1. 检查 initMapApi 是否可用
  if (typeof initMapApi !== 'function') { setTimeout(doInit, 100); return; }
  
  // 2. 临时替换 INIT_LON/INIT_LAT 为保存的位置
  window.INIT_LON = savedLon;
  window.INIT_LAT = savedLat;
  
  // 3. 执行初始化
  initMapApi(newKey);
  
  // 4. 恢复 INIT_LON/INIT_LAT
  // 5. 延迟 200ms 后强制刷新尺寸
  // 6. 恢复路径、预览、追踪等状态
}
```

### 6.5 容器重建

```javascript
// 清空旧容器内容
oldMapDiv.innerHTML = '';
// 从 DOM 移除
oldMapDiv.parentNode.removeChild(oldMapDiv);
// 创建新容器
var newMapDiv = document.createElement('div');
newMapDiv.id = 'map';
newMapDiv.style.cssText = 'width:100%;height:100%';
// 插入到 search-box 之前
searchBox.parentNode.insertBefore(newMapDiv, searchBox);
// 强制 reflow
newMapDiv.offsetHeight;
```

---

## 7. API 端点汇总

| 用途 | 端点 | 方法 | 格式 |
|------|------|------|------|
| 天地图 T API | `https://api.tianditu.gov.cn/api?v=4.0&tk={tk}` | GET | JavaScript |
| 瓦片 (矢量) | `https://t0.tianditu.gov.cn/DataServer?T=vec_w&x={x}&y={y}&l={z}&tk={tk}` | GET | 图片 |
| 瓦片 (矢量注记) | `https://t0.tianditu.gov.cn/DataServer?T=cva_w&x={x}&y={y}&l={z}&tk={tk}` | GET | 图片 |
| 瓦片 (卫星) | `https://t0.tianditu.gov.cn/DataServer?T=img_w&x={x}&y={y}&l={z}&tk={tk}` | GET | 图片 |
| 瓦片 (卫星注记) | `https://t0.tianditu.gov.cn/DataServer?T=cia_w&x={x}&y={y}&l={z}&tk={tk}` | GET | 图片 |
| 搜索 REST | `https://api.tianditu.gov.cn/v2/search?postStr={json}&type=query&tk={tk}` | GET | JSON (JSONP) |
| 驾车规划 REST | `https://api.tianditu.gov.cn/drive?postStr={json}&type=search&tk={tk}` | GET | XML |

---

## 8. 全局变量清单

| 变量名 | 类型 | 用途 | 定义位置 |
|--------|------|------|---------|
| `window._mapApiKey` | string | 当前使用的 API Key | `initMapApi` |
| `window._tiandituApiKey` | string | 天地图专用 Key | `initMapApi` |
| `window._tiandituLayers` | array | 瓦片层引用列表 | `initMapApi` |
| `window._tdtServiceMap` | T.Map / null | 隐藏 T.Map 实例 | `initMapApi` |
| `window._tdtUseNative` | boolean | 原生 T API 可用标志 | `initMapApi` |
| `window._tiandituApiKey` | string | 天地图 Key | 页面加载/Key管理 |
| `window._baiduSwitchCallback` | function | 百度切换回调 | `qtSwitchMapSource` |

---

## 9. 常见问题

### 9.1 天地图 Key 失效

**现象**: 瓦片不加载，显示空白地图。  
**检查**: 日志中 `window._mapApiKey` 是否正确。  
**解决**: 通过 `⚙` 设置按钮重新输入天地图 Key。  
**注意**: 天地图 Key 需要到 [https://console.tianditu.gov.cn](https://console.tianditu.gov.cn) 申请，免费版有调用量限制。

### 9.2 T API 初始化失败

**现象**: 路线规划或搜索失败，日志显示 "