# blec — 树莓派端蓝牙守护进程

基于 [btstack](https://github.com/bluekitchen/btstack) 的 BLE 外设实现，作为 systemd 服务常驻运行。

## 文件

| 文件 | 说明 |
|------|------|
| `ble_daemon.c` | 守护进程主入口，初始化 btstack、注册 GATT、进入事件循环 |
| `blemon.c` | 蓝牙状态监控 |
| `mv999.gatt` | GATT 服务与特征定义（由 btstack gatt 工具编译） |
| `gpssim_pi.c` | GPS 信号生成（树莓派版，含 `socket.c`） |
| `pluto_tx_pi.c` | PlutoSDR 发射（树莓派版，`rf_core.c` 的 Linux 精简移植） |
| `Makefile` | 构建脚本 |
| `blec.service` | systemd 服务单元 |

## 依赖

- **btstack** — 默认路径 `/home/gps/btstack`，改 `Makefile` 中的 `BTSTACK_ROOT`
- **zlib** — `ble_daemon.c` 需要（星历解压）
- **libiio** — `pluto_tx_pi.c` 需要（PlutoSDR）

## 编译

```bash
cd BLEManager/blec
make
```

`pluto_tx_pi.c` 单独编译：

```bash
gcc -O3 -o pluto_tx pluto_tx.c -liio -lm
```

## 部署

```bash
sudo cp blec.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now blec
```

### 服务要点

| 配置 | 值 | 说明 |
|------|-----|------|
| `ExecStartPre` | `/usr/bin/hciconfig hci0 down` | 释放 HCI，交给 btstack 独占 |
| `Conflicts` | `bluetooth.service` 等 | 与系统蓝牙服务互斥 |
| `ExecStart` | `/home/gps/blec/ble_daemon` | 可执行路径 |
| `Restart` | `always` / `RestartSec=3` | 崩溃自动拉起 |
| `User` | `root` | 操作 HCI 需 root |

> 必须停掉系统自带 `bluetooth.service`：btstack 直接操作 HCI 设备，两者共存会抢占失败。

## 与设备通信

GATT 服务与特征定义在 `mv999.gatt`，编译后生成对应 C 代码注册到 btstack。手机端（`BLEManager`）按同一份定义读写。

详见 [../../docs/ble.md](../../docs/ble.md)。
