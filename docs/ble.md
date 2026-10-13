# 蓝牙星历传输 (BLE)

用于把星历数据从手机/上位机经蓝牙传给树莓派端设备。

## 组成

```
BLEManager/                 iOS / macOS 端 (Objective-C, CoreBluetooth)
├── BLEManager.h/.m             蓝牙管理主类
└── CBPeripheral+Peripherals.h/.m 外设分类扩展

BLEManager/blec/            树莓派端 (C, btstack)
├── ble_daemon.c                蓝牙守护进程入口
├── blemon.c                    状态监控
├── gpssim_pi.c                 信号生成（树莓派版）
├── pluto_tx_pi.c               PlutoSDR 发射（树莓派版）
├── mv999.gatt                  GATT 服务/特征定义
├── blec.service                systemd 服务单元
└── Makefile                    构建脚本
```

## 树莓派端 (blec)

基于 [btstack](https://github.com/bluekitchen/btstack) 实现 BLE 外设。

### 依赖

- btstack（默认路径 `/home/gps/btstack`，在 `Makefile` 中修改 `BTSTACK_ROOT`）
- zlib
- libiio（`pluto_tx_pi.c` 需要）

### 编译

```bash
cd BLEManager/blec
make
```

### 部署为服务

`blec.service` 已写好 systemd 单元：

| 配置 | 值 |
|------|-----|
| 描述 | MV999 BLE Daemon (btstack C) |
| 冲突服务 | `bluetooth.service`、`ble-server.service`、`ble-watchdog.service`、`debug-server.service` |
| 启动前 | `hciconfig hci0 down`（释放 HCI 给 btstack 独占） |
| 可执行 | `/home/gps/blec/ble_daemon` |
| 重启策略 | `always`，间隔 3 秒 |
| 用户 | `root` |

安装：

```bash
sudo cp blec.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now blec
```

> 注意：btstack 直接操作 HCI，必须先停掉系统自带 `bluetooth.service`，否则抢占失败。

### GATT 协议

服务与特征定义在 `mv999.gatt`，由 btstack 的 gatt 工具生成对应 C 代码。

## iOS / macOS 端 (BLEManager)

`BLEManager` 封装 CoreBluetooth，提供连接、读写特征、订阅通知等能力。

错误码定义（`BleError`）：

| 值 | 名称 | 含义 |
|----|------|------|
| 100 | `BleErrorBluetoothDisable` | 蓝牙未打开 |
| 101 | `BleErrorBluetoothUnsupported` | 设备不支持蓝牙 |
| 102 | `BleErrorBluetoothUnauthorized` | 蓝牙未授权 |
| 200 | `BleErrorReadCharacteristicNil` | 读特征值为空 |
| 201 | `BleErrorWriteCharacteristicNil` | 写特征值为空 |
| 202 | `BleErrorSetNotifyingFailed` | 设置监听失败 |
| 203 | `BleErrorReceiveDataTimeOut` | 接收数据超时 |

通过 `BLEManagerDelegate` 回调返回结果。
