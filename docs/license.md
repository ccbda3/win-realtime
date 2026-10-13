# 授权机制

## 设计

采用 **RSA-2048 非对称签名**：客户端只内嵌公钥用于验证，私钥仅存在于签发工具中。

- 签名算法：PKCS#1 v1.5 + SHA-256
- 密钥长度：2048 位
- 载荷格式：`机器码|类型|时间戳`

## 授权文件

授权文件（`*.lic`）内容为签名数据，客户端流程：

1. 读取本机机器码（32 位十六进制）
2. 从授权文件解析 `机器码|类型|时间戳` 与签名
3. 用内嵌公钥验签
4. 校验机器码是否匹配本机
5. 按类型判断有效期

## 授权类型

| 类型 | 说明 |
|------|------|
| `24h` | 24 小时有效期 |
| `48h` | 48 小时有效期 |
| `perm` | 永久 |

## 客户端源码

`gps-ui-qt/gps-ui-qt/src/rsa_verify.c` / `.h`

- 使用 Windows CNG（`bcrypt`）做 RSA 验签
- **只包含公钥**，无任何私钥材料
- 验证逻辑同时做了 PE 头 / 文件完整性检查

## 签发工具（不在本仓库）

`license_gen/` 与 `license_gen_android/` 为内部签发工具，**未开源**，原因：

- 它们内嵌 RSA **私钥**（明文 PEM + XOR 混淆版），一旦公开任何人都能伪造授权码
- 涉及商业授权策略

签发工具使用方需自行保管私钥；如私钥泄露，须立即重新生成密钥对并更新客户端公钥。

## 相关接口

`main.cpp` 中暴露给主窗口的授权状态接口：

```c
void    setAuthPermanent(int v);
int     getAuthPermanent();
void    setAuthRemaining(double v);
double  getAuthRemaining();
void    setStoredSig(const QString &v);
QString getStoredSig();
QString reActivateLicense(const QString &filePath);
```
