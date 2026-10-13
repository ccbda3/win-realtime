# SDR 设备支持

设备抽象层位于 `gps-ui-qt/gps-ui-qt/src/core/rf_core.c`，对外暴露统一接口：

```c
typedef void (*iq_callback)(float *data, int n_floats, void *user);
typedef void (*log_callback)(const char *msg, void *user);

typedef struct {
    iq_callback  on_iq;
    log_callback on_log;
    void       (*on_disconnect)(void *user);
    void        *user;
    volatile int running;
    int          power_level;
    char         device_name[64];
} rf_context;

int rf_run(rf_context *ctx);
```

上层只需构造 `rf_context` 并调用 `rf_run()`，具体设备由 `device_name` 决定。

## 支持的设备

### HackRF

- **动态加载** `hackrf.dll`（`LoadLibrary` + `GetProcAddress`），编译期无需 SDK
- 使用环形缓冲区做 TX：`HACKRF_RING_SAMPS = BUF_SAMPS * 64`
- 缓冲区读写带超时保护（`hackrf_ring_write/read`）
- 关键参数：采样率、中心频率、TX VGA 增益、AMP 使能、基带滤波带宽

相关函数指针：`hackrf_init / open / start_tx / stop_tx / set_sample_rate / set_freq / set_txvga_gain / set_amp_enable / set_antenna_enable / set_baseband_filter_bandwidth`

### USRP (UHD)

- 链接 `uhd.lib`，编译需定义 `HAS_UHD`
- 通过 UHD 流 API 发送
- 未安装 SDK 时该分支不参与编译

### PlutoSDR

- AD936x 平台
- Windows 端通过 libiio；树莓派端有独立移植版（见下）

## 参数常量

| 常量 | 值 | 含义 |
|------|-----|------|
| `BUF_SAMPS` | 25600 | 单次缓冲采样数 |
| `HACKRF_RING_SAMPS` | `BUF_SAMPS * 64` | HackRF 环形缓冲容量 |
| `CAL_BLOCKS` | 100 | 发射前校准块数 |
| `CAL_HEADROOM` | 0.95f | 校准余量系数 |

## 树莓派移植版

`BLEManager/blec/pluto_tx_pi.c` 是 `rf_core.c` 的 Linux 精简移植：

- 只保留 PlutoSDR（直接链接 `libiio`，无运行时动态加载）
- 编译：`gcc -O3 -o pluto_tx pluto_tx.c -liio -lm`
- 配合 `gpssim_pi.c` 在树莓派上跑信号生成

详见 [BLEManager/blec/README.md](../BLEManager/blec/README.md)。

## 功率等级

`ctx.power_level` 控制发射功率，上层下拉框可选。不同设备映射到各自的增益参数（HackRF → TX VGA，USRP → 增益 dB）。
