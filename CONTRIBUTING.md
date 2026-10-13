# 贡献指南

## 提交前检查

1. **不要提交密钥** — 以下文件已在 `.gitignore` 中，请勿强制添加：
   - `hotupdate/r2_credentials.h`（R2 凭证）
   - `api.txt`
   - `*.pem` / `*.key` / `*.lic`
   - `license_gen/`、`license_gen_android/`（含 RSA 私钥）
   - `gen_c_arrays.py`、`license_gen/decode_key.py`

2. **不要提交构建产物** — `build/`、`*_portable/`、`installer_output/`、`*.exe`、`*.dll` 等已忽略。

3. **代码风格**
   - C/C++：与现有文件保持一致；所有可执行文件用 `WIN32` + `/SUBSYSTEM:WINDOWS`，避免黑色 cmd 窗口
   - 中文源码文件按 GBK 保存，编译加 `/utf-8`（或统一 UTF-8 并全程带 `/utf-8`）

## 提交信息

采用 `类型: 简述` 格式：

| 类型 | 用途 |
|------|------|
| `feat` | 新功能 |
| `fix` | 缺陷修复 |
| `docs` | 文档 |
| `refactor` | 重构 |
| `build` | 构建 / 打包 |
| `chore` | 杂项 |

示例：

```
fix: 修复 HackRF 环形缓冲在高采样率下溢出
docs: 补充四联图绘制参数说明
```

## 新增文档

- 模块级说明放在该模块目录下的 `README.md`
- 全局性文档放 `docs/`，并在 [docs/README.md](docs/README.md) 索引中登记

## 报告问题

提交 Issue 时请附上：

- Windows 版本、Qt 版本
- 使用的 SDR 设备型号
- 复现步骤与完整日志（程序目录下 `.log` 文件）
