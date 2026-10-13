# BLEManager — 蓝牙星历传输

把星历数据经蓝牙从手机/上位机传输到设备端，分两部分：**iOS/macOS 端 SDK** 与 **树莓派端服务**。

## 目录

```
BLEManager/
├── BLEManager.h / .m                iOS/macOS 蓝牙管理主类（CoreBluetooth）
├── CBPeripheral+Peripherals.h / .m  外设分类扩展（读写特征封装）
└── blec/                            树莓派端 btstack 蓝牙守护进程
    └── 详见 blec/README.md
```

## iOS / macOS 端

基于 `CoreBluetooth` 封装，提供扫描、连接、读写特征、订阅通知能力。

### 错误码

| 值 | 名称 | 含义 |
|----|------|------|
| 100 | `BleErrorBluetoothDisable` | 蓝牙未打开 |
| 101 | `BleErrorBluetoothUnsupported` | 设备不支持蓝牙 |
| 102 | `BleErrorBluetoothUnauthorized` | 蓝牙未授权 |
| 200 | `BleErrorReadCharacteristicNil` | 读特征值为空 |
| 201 | `BleErrorWriteCharacteristicNil` | 写特征值为空 |
| 202 | `BleErrorSetNotifyingFailed` | 设置监听失败 |
| 203 | `BleErrorReceiveDataTimeOut` | 接收数据超时 |

通过 `BLEManagerDelegate` 协议回调返回结果。

### 使用

```objc
#import "BLEManager.h"

BLEManager *mgr = [[BLEManager alloc] init];
mgr.delegate = self;
[mgr startScan];   // 开始扫描外设
```

## 树莓派端

见 [blec/README.md](blec/README.md)。

## 详细文档

[../docs/ble.md](../docs/ble.md)
