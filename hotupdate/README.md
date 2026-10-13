# hotupdate — 热更新组件

通过 Cloudflare R2 实现客户端自动更新，以及渲染后端切换工具。

## 文件

| 文件 | 产物 | 说明 |
|------|------|------|
| `updater.c` | `updater.exe` | 主更新器：拉取 version.json → 比对 MD5 → 增量下载 → 替换 |
| `push_update.c` | 上传工具 | 计算 MD5、生成 version.json、推送产物到 R2 |
| `toggle_render.c` | `切换CPU渲染.exe` `切换GPU渲染.exe` | 修改 QtWebEngine 渲染后端 |
| `r2_credentials.h.example` | — | 凭证模板（复制为 `r2_credentials.h` 使用） |
| `stage/version.json` | — | 版本清单示例 |

## 凭证

`r2_credentials.h` **不入库**：

```bash
cp r2_credentials.h.example r2_credentials.h
```

填入自己的 Cloudflare R2 凭证后即可编译 `push_update.c`。

## 更新清单格式

见 [../docs/hotupdate.md](../docs/hotupdate.md)。

## 日志

更新器在自身目录写 `updater.log`，排障优先查看。
