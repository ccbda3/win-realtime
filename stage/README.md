# stage — 打包清单与版本模板

发布阶段的清单文件与版本号模板，供 `hotupdate/push_update` 与安装脚本使用。

## 文件

| 文件 | 说明 |
|------|------|
| `version.json` | 版本清单（版本号、更新说明、文件 MD5 列表） |
| `version.txt` | 纯文本版本号 |
| `manifest_quanneng.txt` | 全能版打包清单 |
| `manifest_gaoduan.txt` | 高端版打包清单 |
| `remove_files.txt` | 需要从旧版本删除的文件 |
| `extra_files.txt` | 额外附加文件 |

## manifest 格式

每行一条，管道符分隔：

```
文件名|MD5
```

示例：

```
gps-ui-qt.exe|e9773211c14068fdada4aa7c9baaf8c4
gps-core.exe|683be6569d7b1081e293f9c4eef2bb68
static/qtmap.html|b977a3ec4cf9be483043d684013027fb
```

含子目录的路径用 `/` 分隔。

## version.json 格式

```json
{
  "version": 10013,
  "version_str": "1.0.13",
  "note": "更新说明",
  "files": [ { "path": "...", "size": 0, "md5": "..." } ],
  "remove": []
}
```

生成方式见 [../docs/hotupdate.md](../docs/hotupdate.md)。
