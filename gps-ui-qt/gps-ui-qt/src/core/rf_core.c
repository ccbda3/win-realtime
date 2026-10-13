#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <winsock2.h>
#include <windows.h>
#include <uhd.h>

#define BUF_SAMPS 25600
#define HACKRF_RING_SAMPS (BUF_SAMPS * 64)
#define CAL_BLOCKS 100
#define CAL_HEADROOM 0.95f

typedef void (*iq_callback)(float *data, int n_floats, void *user);
typedef void (*log_callback)(const char *msg, void *user);

typedef struct {
    iq_callback on_iq;
    log_callback on_log;
    void (*on_disconnect)(void *user);
    void *user;
    volatile int running;
    int power_level; // 0-10 slider value, each SDR maps to its own dB range
    char device_name[64];
} rf_context;

static void rf_log(rf_context *ctx, const char *msg) {
    if (ctx->on_log) ctx->on_log(msg, ctx->user);
}

/* ========== HackRF ring buffer ========== */
typedef struct {
    int8_t *data;
    int capacity;
    volatile int write_pos;
    volatile int read_pos;
    HANDLE can_write;
    HANDLE can_read;
    CRITICAL_SECTION lock;
} hackrf_ring_t;

static hackrf_ring_t *hackrf_ring_create(int samples) {
    hackrf_ring_t *r = (hackrf_ring_t *)malloc(sizeof(hackrf_ring_t));
    r->data = (int8_t *)malloc(samples * 2 * sizeof(int8_t));
    r->capacity = samples;
    r->write_pos = 0;
    r->read_pos = 0;
    r->can_write = CreateSemaphoreA(NULL, samples, samples, NULL);
    r->can_read = CreateSemaphoreA(NULL, 0, samples, NULL);
    InitializeCriticalSection(&r->lock);
    return r;
}

static void hackrf_ring_destroy(hackrf_ring_t *r) {
    CloseHandle(r->can_write);
    CloseHandle(r->can_read);
    DeleteCriticalSection(&r->lock);
    free(r->data);
    free(r);
}

static int hackrf_ring_write(hackrf_ring_t *r, const int8_t *data, int samples, int timeout_ms) {
    DWORD dw = WaitForSingleObject(r->can_write, timeout_ms);
    if (dw != WAIT_OBJECT_0) return 0;
    EnterCriticalSection(&r->lock);
    int wp = r->write_pos;
    int cap = r->capacity;
    if (wp + samples <= cap) {
        memcpy(r->data + wp * 2, data, samples * 2);
    } else {
        int first = cap - wp;
        memcpy(r->data + wp * 2, data, first * 2);
        memcpy(r->data, data + first * 2, (samples - first) * 2);
    }
    r->write_pos = (wp + samples) % cap;
    LeaveCriticalSection(&r->lock);
    ReleaseSemaphore(r->can_read, samples, NULL);
    return samples;
}

static int hackrf_ring_read(hackrf_ring_t *r, int8_t *data, int samples) {
    DWORD dw = WaitForSingleObject(r->can_read, 1000);
    if (dw != WAIT_OBJECT_0) return 0;
    EnterCriticalSection(&r->lock);
    int rp = r->read_pos;
    int cap = r->capacity;
    if (rp + samples <= cap) {
        memcpy(data, r->data + rp * 2, samples * 2);
    } else {
        int first = cap - rp;
        memcpy(data, r->data + rp * 2, first * 2);
        memcpy(data + first * 2, r->data, (samples - first) * 2);
    }
    r->read_pos = (rp + samples) % cap;
    LeaveCriticalSection(&r->lock);
    ReleaseSemaphore(r->can_write, samples, NULL);
    return samples;
}

/* ========== HackRF native TX ========== */
/* Dynamic linking typedefs */
typedef int (__cdecl *fn_hackrf_init)(void);
typedef int (__cdecl *fn_hackrf_exit)(void);
typedef int (__cdecl *fn_hackrf_open)(void**);
typedef int (__cdecl *fn_hackrf_close)(void*);
typedef int (__cdecl *fn_hackrf_start_tx)(void*, void*, void*);
typedef int (__cdecl *fn_hackrf_stop_tx)(void*);
typedef int (__cdecl *fn_hackrf_set_sample_rate)(void*, double);
typedef int (__cdecl *fn_hackrf_set_freq)(void*, uint64_t);
typedef int (__cdecl *fn_hackrf_set_txvga_gain)(void*, uint32_t);
typedef int (__cdecl *fn_hackrf_set_amp_enable)(void*, uint8_t);
typedef int (__cdecl *fn_hackrf_set_antenna_enable)(void*, uint8_t);
typedef int (__cdecl *fn_hackrf_set_baseband_filter_bandwidth)(void*, uint32_t);
typedef const char* (__cdecl *fn_hackrf_error_name)(int);
typedef int (__cdecl *fn_hackrf_is_streaming)(void*);

/* Minimal hackrf_transfer struct for the TX callback */
typedef struct {
    void *device;
    uint8_t *buffer;
    int buffer_length;
    int valid_length;
    void *rx_ctx;
    void *tx_ctx;
} hackrf_transfer_t;

typedef struct {
    void *dev;
    hackrf_ring_t *ring;
    volatile int callback_running;
} hackrf_tx_ctx_t;

static int __cdecl hackrf_tx_callback(hackrf_transfer_t *t) {
    hackrf_tx_ctx_t *tc = (hackrf_tx_ctx_t *)t->tx_ctx;
    int samples_needed = t->valid_length / 2;
    int got = hackrf_ring_read(tc->ring, (int8_t *)t->buffer, samples_needed);
    if (got < samples_needed) {
        memset(t->buffer + got * 2, 0, (samples_needed - got) * 2);
    }
    return 0;
}

static int run_hackrf(rf_context *ctx, SOCKET gpssim, SOCKET srv) {
    HMODULE dll = LoadLibraryA("hackrf-0.dll");
    if (!dll) {
        rf_log(ctx, "无法加载 hackrf-0.dll，请安装 HackRF 驱动");
        return 1;
    }
    fn_hackrf_init h_init = (fn_hackrf_init)GetProcAddress(dll, "hackrf_init");
    fn_hackrf_open h_open = (fn_hackrf_open)GetProcAddress(dll, "hackrf_open");
    fn_hackrf_close h_close = (fn_hackrf_close)GetProcAddress(dll, "hackrf_close");
    fn_hackrf_start_tx h_start_tx = (fn_hackrf_start_tx)GetProcAddress(dll, "hackrf_start_tx");
    fn_hackrf_stop_tx h_stop_tx = (fn_hackrf_stop_tx)GetProcAddress(dll, "hackrf_stop_tx");
    fn_hackrf_set_sample_rate h_set_sr = (fn_hackrf_set_sample_rate)GetProcAddress(dll, "hackrf_set_sample_rate");
    fn_hackrf_set_freq h_set_freq = (fn_hackrf_set_freq)GetProcAddress(dll, "hackrf_set_freq");
    fn_hackrf_set_txvga_gain h_set_txvga = (fn_hackrf_set_txvga_gain)GetProcAddress(dll, "hackrf_set_txvga_gain");
    fn_hackrf_set_amp_enable h_set_amp = (fn_hackrf_set_amp_enable)GetProcAddress(dll, "hackrf_set_amp_enable");
    fn_hackrf_set_antenna_enable h_set_ant = (fn_hackrf_set_antenna_enable)GetProcAddress(dll, "hackrf_set_antenna_enable");
    fn_hackrf_set_baseband_filter_bandwidth h_set_bw = (fn_hackrf_set_baseband_filter_bandwidth)GetProcAddress(dll, "hackrf_set_baseband_filter_bandwidth");
    fn_hackrf_error_name h_err_name = (fn_hackrf_error_name)GetProcAddress(dll, "hackrf_error_name");

    if (!h_init || !h_open || !h_close || !h_start_tx || !h_stop_tx || !h_set_sr || !h_set_freq || !h_set_txvga) {
        rf_log(ctx, "hackrf-0.dll 版本不兼容，缺少必要函数");
        FreeLibrary(dll);
        return 1;
    }

    int ret = h_init();
    if (ret != 0) {
        rf_log(ctx, "hackrf_init 失败");
        FreeLibrary(dll);
        return 1;
    }

    void *dev = NULL;
    ret = h_open(&dev);
    if (ret != 0 || !dev) {
        rf_log(ctx, "hackrf_open 失败，请确认 HackRF 已连接");
        h_init = NULL;
        h_init = (fn_hackrf_init)GetProcAddress(dll, "hackrf_exit");
        if (h_init) ((fn_hackrf_init)h_init)();
        FreeLibrary(dll);
        return 1;
    }

    h_set_sr(dev, 2560000.0);
    h_set_freq(dev, 1575420000ULL);
    int hgain = 20 + (ctx->power_level > 10 ? 10 : ctx->power_level) * 27 / 10;
    if (hgain > 47) hgain = 47; if (hgain < 20) hgain = 20;
    h_set_txvga(dev, hgain);
    if (h_set_amp) h_set_amp(dev, 1);
    if (h_set_ant) h_set_ant(dev, 1);
    if (h_set_bw) h_set_bw(dev, 3500000);

    hackrf_tx_ctx_t tx_ctx;
    tx_ctx.dev = dev;
    tx_ctx.ring = hackrf_ring_create(HACKRF_RING_SAMPS);
    tx_ctx.callback_running = 1;

    ret = h_start_tx(dev, (void*)hackrf_tx_callback, &tx_ctx);
    if (ret != 0) {
        rf_log(ctx, "hackrf_start_tx 失败");
        hackrf_ring_destroy(tx_ctx.ring);
        h_close(dev);
        ((fn_hackrf_exit)GetProcAddress(dll, "hackrf_exit"))();
        FreeLibrary(dll);
        return 1;
    }

    rf_log(ctx, "HackRF 已发射 (1575.42MHz, 2.6Msps)");

    float *iq = (float *)malloc(BUF_SAMPS * 2 * sizeof(float));
    int total = BUF_SAMPS * 2 * (int)sizeof(float);
    int8_t *iq8 = (int8_t *)malloc(BUF_SAMPS * 2 * sizeof(int8_t));

    while (ctx->running) {
        int got = 0;
        int disconnected = 0;
        while (got < total && ctx->running) {
            int r = recv(gpssim, ((char *)iq) + got, total - got, 0);
            if (r > 0) { got += r; continue; }
            if (r == 0) { disconnected = 1; break; }
            if (WSAGetLastError() == WSAETIMEDOUT) {
                if (!ctx->running) break;
                continue;
            }
            disconnected = 1; break;
        }
        if (!ctx->running) break;
        if (disconnected) break;
        float maxVal = 1e-6f;
        for (int ni = 0; ni < BUF_SAMPS * 2; ni++) { float a = fabsf(iq[ni]); if (a > maxVal) maxVal = a; }
        float scale = 1.0f / maxVal;
        for (int i = 0; i < BUF_SAMPS * 2; i++) {
            float v = iq[i] * scale;
            iq8[i] = (int8_t)(v * 127.0f);
            iq[i] = v;
        }
        if (ctx->on_iq) ctx->on_iq(iq, BUF_SAMPS * 2, ctx->user);
        hackrf_ring_write(tx_ctx.ring, iq8, BUF_SAMPS, 1000);
    }

    free(iq8);
    free(iq);
    tx_ctx.callback_running = 0;
    h_stop_tx(dev);
    hackrf_ring_destroy(tx_ctx.ring);
    h_close(dev);
    ((fn_hackrf_exit)GetProcAddress(dll, "hackrf_exit"))();
    FreeLibrary(dll);
    rf_log(ctx, "HackRF 已停止");
    return 0;
}

/* ========== PlutoSDR native TX (libiio) ========== */
typedef struct iio_context* (__cdecl *fn_iio_create_context_from_uri)(const char*);
typedef struct iio_context* (__cdecl *fn_iio_create_network_context)(const char*);
typedef void (__cdecl *fn_iio_context_destroy)(struct iio_context*);
typedef struct iio_device* (__cdecl *fn_iio_context_find_device)(struct iio_context*, const char*);
typedef struct iio_channel* (__cdecl *fn_iio_device_find_channel)(struct iio_device*, const char*, int);
typedef void (__cdecl *fn_iio_channel_enable)(struct iio_channel*);
typedef ssize_t (__cdecl *fn_iio_device_attr_write_longlong)(struct iio_device*, const char*, long long);
typedef ssize_t (__cdecl *fn_iio_device_attr_read)(struct iio_device*, const char*, char*, size_t);
typedef ssize_t (__cdecl *fn_iio_channel_attr_write_longlong)(struct iio_channel*, const char*, long long);
typedef ssize_t (__cdecl *fn_iio_channel_attr_write)(struct iio_channel*, const char*, const char*);
typedef struct iio_buffer* (__cdecl *fn_iio_device_create_buffer)(struct iio_device*, size_t, int);
typedef void (__cdecl *fn_iio_buffer_destroy)(struct iio_buffer*);
typedef ssize_t (__cdecl *fn_iio_buffer_push)(struct iio_buffer*);
typedef void* (__cdecl *fn_iio_buffer_start)(const struct iio_buffer*);
typedef ptrdiff_t (__cdecl *fn_iio_buffer_step)(const struct iio_buffer*);
typedef int (__cdecl *fn_iio_context_set_timeout)(struct iio_context*, unsigned int);
typedef int (__cdecl *fn_iio_context_get_version)(struct iio_context*, unsigned int*, unsigned int*, unsigned int*);
typedef int (__cdecl *fn_iio_device_set_kernel_buffers_count)(struct iio_device*, unsigned int);
typedef int (__cdecl *fn_ad9361_set_bb_rate)(struct iio_device*, unsigned int);

static int run_pluto(rf_context *ctx, SOCKET gpssim, SOCKET srv) {
    HMODULE dll = LoadLibraryA("libiio.dll");
    if (!dll) {
        rf_log(ctx, "无法加载 libiio.dll，请安装 libiio");
        return 1;
    }
    HMODULE dll_ad = LoadLibraryA("libad9361.dll");
    fn_ad9361_set_bb_rate ad9361_bb_rate = NULL;
    if (dll_ad) {
        ad9361_bb_rate = (fn_ad9361_set_bb_rate)GetProcAddress(dll_ad, "ad9361_set_bb_rate");
        if (ad9361_bb_rate) rf_log(ctx, "libad9361: ad9361_set_bb_rate 可用");
    }

    fn_iio_create_context_from_uri iio_create_ctx = (fn_iio_create_context_from_uri)GetProcAddress(dll, "iio_create_context_from_uri");
    fn_iio_create_network_context iio_create_net = (fn_iio_create_network_context)GetProcAddress(dll, "iio_create_network_context");
    fn_iio_context_destroy iio_destroy_ctx = (fn_iio_context_destroy)GetProcAddress(dll, "iio_context_destroy");
    fn_iio_context_find_device iio_find_dev = (fn_iio_context_find_device)GetProcAddress(dll, "iio_context_find_device");
    fn_iio_device_find_channel iio_find_chan = (fn_iio_device_find_channel)GetProcAddress(dll, "iio_device_find_channel");
    fn_iio_channel_enable iio_chan_enable = (fn_iio_channel_enable)GetProcAddress(dll, "iio_channel_enable");
    fn_iio_device_attr_write_longlong iio_dev_attr_ll = (fn_iio_device_attr_write_longlong)GetProcAddress(dll, "iio_device_attr_write_longlong");
    fn_iio_device_attr_read iio_dev_attr_read = (fn_iio_device_attr_read)GetProcAddress(dll, "iio_device_attr_read");
    fn_iio_channel_attr_write_longlong iio_chan_attr_ll = (fn_iio_channel_attr_write_longlong)GetProcAddress(dll, "iio_channel_attr_write_longlong");
    fn_iio_channel_attr_write iio_chan_attr = (fn_iio_channel_attr_write)GetProcAddress(dll, "iio_channel_attr_write");
    fn_iio_device_create_buffer iio_create_buf = (fn_iio_device_create_buffer)GetProcAddress(dll, "iio_device_create_buffer");
    fn_iio_buffer_destroy iio_destroy_buf = (fn_iio_buffer_destroy)GetProcAddress(dll, "iio_buffer_destroy");
    fn_iio_buffer_push iio_buf_push = (fn_iio_buffer_push)GetProcAddress(dll, "iio_buffer_push");
    fn_iio_buffer_start iio_buf_start = (fn_iio_buffer_start)GetProcAddress(dll, "iio_buffer_start");
    fn_iio_buffer_step iio_buf_step = (fn_iio_buffer_step)GetProcAddress(dll, "iio_buffer_step");
    fn_iio_context_set_timeout iio_set_timeout = (fn_iio_context_set_timeout)GetProcAddress(dll, "iio_context_set_timeout");
    fn_iio_context_get_version iio_get_ver = (fn_iio_context_get_version)GetProcAddress(dll, "iio_context_get_version");
    fn_iio_device_set_kernel_buffers_count iio_set_kbuf = (fn_iio_device_set_kernel_buffers_count)GetProcAddress(dll, "iio_device_set_kernel_buffers_count");

    if (!iio_create_ctx || !iio_destroy_ctx || !iio_find_dev || !iio_find_chan ||
        !iio_chan_enable || !iio_chan_attr_ll ||
        !iio_create_buf || !iio_destroy_buf || !iio_buf_push || !iio_buf_start || !iio_buf_step) {
        rf_log(ctx, "libiio.dll 版本不兼容");
        FreeLibrary(dll);
        return 1;
    }

    struct iio_context *iioc = NULL;
    if (iio_create_net) iioc = iio_create_net("192.168.2.1");
    if (!iioc) iioc = iio_create_ctx("ip:192.168.2.1");
    if (!iioc) {
        rf_log(ctx, "无法连接到 PlutoSDR");
        FreeLibrary(dll);
        return 1;
    }
    iio_set_timeout(iioc, 10000);

    struct iio_device *phy = iio_find_dev(iioc, "ad9361-phy");
    struct iio_device *txdev = iio_find_dev(iioc, "cf-ad9361-dds-core-lpc");
    if (!phy || !txdev) {
        rf_log(ctx, "未找到 PlutoSDR 设备");
        iio_destroy_ctx(iioc);
        FreeLibrary(dll);
        return 1;
    }

    if (iio_set_kbuf) iio_set_kbuf(txdev, 8);

    struct iio_channel *phy_chn = iio_find_chan(phy, "voltage0", 1);
    if (!phy_chn) {
        rf_log(ctx, "未找到 phy 通道");
        iio_destroy_ctx(iioc);
        FreeLibrary(dll);
        return 1;
    }
    if (iio_chan_attr) iio_chan_attr(phy_chn, "rf_port_select", "A");
    iio_chan_attr_ll(phy_chn, "rf_bandwidth", 6000000LL);
    iio_chan_attr_ll(phy_chn, "sampling_frequency", 2560000LL);
    int patten = -20 + (ctx->power_level > 10 ? 10 : ctx->power_level) * 20 / 10;
    if (patten > 0) patten = 0; if (patten < -40) patten = -40;
    iio_chan_attr_ll(phy_chn, "hardwaregain", (long long)patten);

    struct iio_channel *rxlo = iio_find_chan(phy, "altvoltage0", 1);
    if (rxlo && iio_chan_attr) iio_chan_attr(rxlo, "powerdown", "1");

    struct iio_channel *txlo = iio_find_chan(phy, "altvoltage1", 1);
    if (txlo) {
        iio_chan_attr_ll(txlo, "frequency", 1575420000LL);
        if (iio_chan_attr) iio_chan_attr(txlo, "powerdown", "0");
    }

    struct iio_channel *tx0 = iio_find_chan(txdev, "voltage0", 1);
    if (!tx0) tx0 = iio_find_chan(txdev, "altvoltage0", 1);
    struct iio_channel *tx1 = iio_find_chan(txdev, "voltage1", 1);
    if (!tx1) tx1 = iio_find_chan(txdev, "altvoltage1", 1);
    if (!tx0 || !tx1) {
        rf_log(ctx, "未找到 发射通道");
        iio_destroy_ctx(iioc);
        FreeLibrary(dll);
        return 1;
    }
    iio_chan_enable(tx0);
    iio_chan_enable(tx1);

    char tx_rates[128] = {0};
    if (iio_dev_attr_read) iio_dev_attr_read(phy, "tx_path_rates", tx_rates, sizeof(tx_rates));

    Sleep(200);

    int buf_samps = 262144;
    struct iio_buffer *txbuf = iio_create_buf(txdev, buf_samps, 0);
    if (!txbuf) {
        rf_log(ctx, "创建 IIO buffer 失败");
        iio_destroy_ctx(iioc);
        FreeLibrary(dll);
        return 1;
    }

    if (txlo && iio_chan_attr) iio_chan_attr(txlo, "powerdown", "0");

    rf_log(ctx, "PlutoSDR 已发射 (1575.42MHz, 2.6Msps)");

    float *iq = (float *)malloc(BUF_SAMPS * 2 * sizeof(float));
    int total = BUF_SAMPS * 2 * (int)sizeof(float);
    int16_t *iq16 = (int16_t *)malloc(buf_samps * 2 * sizeof(int16_t));
    int buf_bytes = buf_samps * 2 * sizeof(int16_t);
    int buf_pos = 0;
    float cal_peak = 1e-6f;
    int cal_count = 0;
    int check_counter = 0;

    while (ctx->running) {
        int got = 0;
        int disconnected = 0;
        while (got < total && ctx->running) {
            int r = recv(gpssim, ((char *)iq) + got, total - got, 0);
            if (r > 0) { got += r; continue; }
            if (r == 0) { disconnected = 1; break; }
            if (WSAGetLastError() == WSAETIMEDOUT) {
                if (!ctx->running) break;
                continue;
            }
            disconnected = 1; break;
        }
        if (!ctx->running) break;
        if (disconnected) break;
        float maxVal = 1e-6f;
        for (int ni = 0; ni < BUF_SAMPS * 2; ni++) { float a = fabsf(iq[ni]); if (a > maxVal) maxVal = a; }
        if (cal_count < CAL_BLOCKS) {
            if (maxVal > cal_peak) cal_peak = maxVal;
            cal_count++;
        }
        float scale = (cal_count >= CAL_BLOCKS) ? (CAL_HEADROOM / cal_peak) : (1.0f / maxVal);
        for (int i = 0; i < BUF_SAMPS * 2; i++) {
            float v = iq[i] * scale;
            iq[i] = v;
            int16_t s = (int16_t)(v * 32767.0f);
            iq16[buf_pos++] = s;
            if (buf_pos >= buf_samps * 2) {
                void *ptr = iio_buf_start(txbuf);
                memcpy(ptr, iq16, buf_bytes);
                if (iio_buf_push(txbuf) < 0) {
                    if (ctx->on_disconnect) ctx->on_disconnect(ctx->user);
                    ctx->running = 0;
                    break;
                }
                buf_pos = 0;
                if (++check_counter >= 50) {
                    check_counter = 0;
                    unsigned int vmaj = 0, vmin = 0, vgit = 0;
                    if (iio_get_ver && iio_get_ver(iioc, &vmaj, &vmin, &vgit) < 0) {
                        if (ctx->on_disconnect) ctx->on_disconnect(ctx->user);
                        ctx->running = 0;
                        break;
                    }
                }
            }
        }
        if (!ctx->running) break;
        if (ctx->on_iq) ctx->on_iq(iq, BUF_SAMPS * 2, ctx->user);
    }

    if (buf_pos > 0) {
        memset(iq16 + buf_pos, 0, (buf_samps * 2 - buf_pos) * sizeof(int16_t));
        void *ptr = iio_buf_start(txbuf);
        memcpy(ptr, iq16, buf_bytes);
        iio_buf_push(txbuf);
    }

    free(iq16);
    free(iq);
    iio_destroy_buf(txbuf);
    iio_destroy_ctx(iioc);
    FreeLibrary(dll);
    rf_log(ctx, "PlutoSDR 已停止");
    return 0;
}

/* ========== USRP TX (UHD) ========== */
static int run_usrp(rf_context *ctx, SOCKET gpssim, SOCKET srv) {
    uhd_usrp_handle usrp = NULL;
    uhd_tx_streamer_handle tx = NULL;
    uhd_tx_metadata_handle md = NULL;
    size_t max_samps = BUF_SAMPS;
    float *iq = (float *)malloc(BUF_SAMPS * 2 * sizeof(float));

    const char *type_str = "";
    if (strstr(ctx->device_name, "hackrf")) {
    } else if (strstr(ctx->device_name, "pluto")) {
        type_str = "type=pluto";
    } else {
        type_str = "type=b200";
    }
    rf_log(ctx, "正在初始化 USRP 设备...");
    if (uhd_usrp_make(&usrp, type_str) != UHD_ERROR_NONE) {
        rf_log(ctx, "USRP 初始化失败");
        free(iq);
        return 1;
    }
    uhd_usrp_set_tx_rate(usrp, 2560000, 0);
    uhd_tune_request_t tr = { 1575420000.0, UHD_TUNE_REQUEST_POLICY_AUTO, UHD_TUNE_REQUEST_POLICY_AUTO };
    uhd_tune_result_t tuner;
    uhd_usrp_set_tx_freq(usrp, &tr, 0, &tuner);
    int ugain = 30 + (ctx->power_level > 10 ? 10 : ctx->power_level) * 60 / 10;
    if (ugain > 90) ugain = 90; if (ugain < 30) ugain = 30;
    uhd_usrp_set_tx_gain(usrp, ugain, 0, "");
    uhd_usrp_set_tx_antenna(usrp, "TX/RX", 0);
    uhd_usrp_set_tx_bandwidth(usrp, 2560000, 0);

    uhd_stream_args_t sa;
    sa.cpu_format = "fc32";
    sa.otw_format = "sc16";
    sa.args = "";
    size_t ch_list = 0;
    sa.channel_list = &ch_list;
    sa.n_channels = 1;
    uhd_tx_streamer_make(&tx);
    uhd_usrp_get_tx_stream(usrp, &sa, tx);
    uhd_tx_streamer_max_num_samps(tx, &max_samps);
    uhd_tx_metadata_make(&md, 0, 0, 0, 0, 0);
    rf_log(ctx, "USRP 就绪，开始流传输...");

    while (ctx->running) {
        int total = BUF_SAMPS * 2 * (int)sizeof(float);
        int got = 0, disconnected = 0;
        while (got < total && ctx->running) {
            int r = recv(gpssim, ((char *)iq) + got, total - got, 0);
            if (r > 0) { got += r; continue; }
            if (r == 0) { disconnected = 1; break; }
            if (WSAGetLastError() == WSAETIMEDOUT) { if (!ctx->running) break; continue; }
            disconnected = 1; break;
        }
        if (!ctx->running || disconnected) break;
        float maxVal = 1e-6f;
        for (int ni = 0; ni < BUF_SAMPS * 2; ni++) { float a = fabsf(iq[ni]); if (a > maxVal) maxVal = a; }
        float scale = 1.0f / maxVal;
        for (int ni = 0; ni < BUF_SAMPS * 2; ni++) iq[ni] *= scale;
        if (ctx->on_iq) ctx->on_iq(iq, BUF_SAMPS * 2, ctx->user);
        int sent = 0;
        while (sent < BUF_SAMPS) {
            int chunk = (BUF_SAMPS - sent) < (int)max_samps ? (BUF_SAMPS - sent) : (int)max_samps;
            const void *buffs[1] = { iq + sent * 2 };
            size_t ns = chunk, sent_s = 0;
            uhd_tx_streamer_send(tx, buffs, ns, &md, 1.0, &sent_s);
            sent += (int)sent_s;
        }
    }

    free(iq);
    uhd_tx_metadata_free(&md);
    uhd_tx_streamer_free(&tx);
    uhd_usrp_free(&usrp);
    rf_log(ctx, "USRP 已停止");
    return 0;
}

/* ========== rf_detect / rf_run ========== */
const char* rf_detect(void) {
    uhd_usrp_handle h = NULL;
    if (uhd_usrp_make(&h, "") == UHD_ERROR_NONE) {
        uhd_usrp_free(&h);
        return "uhd";
    }
    h = NULL;
    if (uhd_usrp_make(&h, "type=pluto") == UHD_ERROR_NONE) {
        uhd_usrp_free(&h);
        return "pluto";
    }
    h = NULL;
    if (uhd_usrp_make(&h, "type=b200") == UHD_ERROR_NONE) {
        uhd_usrp_free(&h);
        return "b210";
    }
    HMODULE dll = LoadLibraryA("hackrf-0.dll");
    if (dll) {
        fn_hackrf_init h_init = (fn_hackrf_init)GetProcAddress(dll, "hackrf_init");
        fn_hackrf_open h_open = (fn_hackrf_open)GetProcAddress(dll, "hackrf_open");
        fn_hackrf_close h_close = (fn_hackrf_close)GetProcAddress(dll, "hackrf_close");
        fn_hackrf_exit h_exit = (fn_hackrf_exit)GetProcAddress(dll, "hackrf_exit");
        if (h_init && h_open && h_close && h_exit && h_init() == 0) {
            void *dev = NULL;
            if (h_open(&dev) == 0 && dev) {
                h_close(dev);
                h_exit();
                FreeLibrary(dll);
                return "hackrf";
            }
            h_exit();
        }
        FreeLibrary(dll);
    }
    return NULL;
}

int rf_run(rf_context *ctx) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    setsockopt(srv, IPPROTO_TCP, TCP_NODELAY, (const char *)&opt, sizeof(opt));
    setsockopt(srv, SOL_SOCKET, SO_KEEPALIVE, (const char *)&opt, sizeof(opt));
    int rcvbuf = 262144;
    setsockopt(srv, SOL_SOCKET, SO_RCVBUF, (const char *)&rcvbuf, sizeof(rcvbuf));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(1999);
    a.sin_addr.s_addr = INADDR_ANY;
    if (bind(srv, (struct sockaddr *)&a, sizeof(a)) < 0) {
        rf_log(ctx, "无法绑定 TCP :1999");
        closesocket(srv); WSACleanup(); return 1;
    }
    listen(srv, 1);

    while (ctx->running) {
        rf_log(ctx, "等待 gpssim 连接 TCP :1999...");
        SOCKET gpssim = INVALID_SOCKET;
        while (ctx->running) {
            fd_set rfds; FD_ZERO(&rfds); FD_SET(srv, &rfds);
            struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 500000;
            int sr = select(0, &rfds, NULL, NULL, &tv);
            if (sr > 0 && FD_ISSET(srv, &rfds)) {
                gpssim = accept(srv, NULL, NULL);
                break;
            }
        }
        if (!ctx->running) break;
        rf_log(ctx, "gpssim 已连接");
        DWORD recv_timeout = 500;
        setsockopt(gpssim, SOL_SOCKET, SO_RCVTIMEO, (const char*)&recv_timeout, sizeof(recv_timeout));

        int ret = 0;
        if (strstr(ctx->device_name, "hackrf")) {
            ret = run_hackrf(ctx, gpssim, srv);
        } else if (strstr(ctx->device_name, "pluto")) {
            ret = run_pluto(ctx, gpssim, srv);
        } else {
            ret = run_usrp(ctx, gpssim, srv);
        }

        closesocket(gpssim);
        if (ret != 0) {
            rf_log(ctx, "设备启动失败，停止重试");
            break;
        }
        if (!ctx->running) break;
        rf_log(ctx, "gpssim 已断开，等待重连...");
    }

    closesocket(srv);
    WSACleanup();
    return 0;
}
