# env_check — 环境检测工具

单文件 Windows 环境检测程序，用于排查部署环境问题，输出检测报告。

## 源码

`env_check.c`（单文件，C，无第三方依赖）

## 检测内容

- **网络** — 使用 `iphlpapi` / `icmpapi` 检查网卡、IP、连通性
- **进程** — 使用 `tlhelp32` 快照枚举进程，检查冲突程序
- **系统** — Windows 版本、权限、必要运行库
- **路径** — 使用 `shlobj` 定位配置目录

## 依赖库

```
ws2_32  iphlpapi  user32  shell32  advapi32
```

## 编译

```bash
cl env_check.c /Fe:env_check.exe /O2 /MT /utf-8
```

## 输出

生成检测报告文件（`g_rep` 文件句柄写入），供技术支持分析。
