# 热更新与发布

## 工作原理

客户端启动时向 Cloudflare R2 拉取 `version.json`，与本机版本比对，按文件 MD5 增量下载替换。

## 版本清单 `version.json`

```json
{
  "version": 10013,
  "version_str": "1.0.13",
  "note": "本次更新说明",
  "files": [
    { "path": "gps-ui-qt.exe", "size": 514560, "md5": "e9773211..." },
    { "path": "static/qtmap.html", "size": 98319, "md5": "b977a3ec..." }
  ],
  "remove": []
}
```

| 字段 | 说明 |
|------|------|
| `version` | 整数版本号，用于比较大小 |
| `version_str` | 展示用版本字符串 |
| `note` | 更新说明，客户端展示 |
| `files[].path` | 相对路径（含子目录） |
| `files[].size` | 字节数 |
| `files[].md5` | 文件 MD5，客户端校验并判断是否需要下载 |
| `remove[]` | 需要删除的旧文件列表 |

`stage/version.json` 是打包阶段的模板，`hotupdate/stage/version.json` 为运行示例。

## 组件

| 文件 | 产物 | 职责 |
|------|------|------|
| `updater.c` | `updater.exe` | 主更新器：读清单 → 比对 → 下载 → 校验 MD5 → 替换 → 重启 |
| `push_update.c` | 上传工具 | 生成 `version.json` 并把产物推到 R2 桶 |
| `toggle_render.c` | `切换CPU渲染.exe` / `切换GPU渲染.exe` | 修改 QtWebEngine 渲染标志，切换 CPU/GPU 渲染 |

## 凭证配置

`push_update.c` 通过 `#include "r2_credentials.h"` 读取凭证，该文件**不入库**：

```bash
cp r2_credentials.h.example r2_credentials.h
```

```c
#define R2_ACCESS_KEY "your-r2-access-key-id"
#define R2_SECRET     "your-r2-secret-access-key"
#define R2_ENDPOINT   "your-account-id.r2.cloudflarestorage.com"
#define R2_BUCKET     "your-bucket-name"
```

> 凭证泄露会导致 bucket 被任意读写，请勿提交、勿写入安装包。

## 发布流程

1. 编译新版本，产物放进 portable 目录
2. 运行 `push_update.exe` 上传文件到 R2
3. 生成并上传新的 `version.json`
4. 客户端下次启动自动更新

## 渲染后端切换

`toggle_render.c` 通过 `Toolhelp32` 快照找到 `gps-ui-qt.exe` 进程路径，修改其 QtWebEngine 配置项（软件渲染 ↔ 硬件渲染）。打包时分发为两个不同名字的 exe，用户双击即切换。

## 日志

更新器在同目录写 `updater.log`，排查失败时优先查看。
