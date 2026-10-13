/* ========== pluto_tx.c - PlutoSDR transmitter for Raspberry Pi ==========
 * Linux port of rf_core.c - stripped down to only PlutoSDR via libiio
 * Directly linked to libiio (no runtime DLL loading)
 * Compile: gcc -O3 -o pluto_tx pluto_tx.c -liio -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <iio.h>

#define BUF_SAMPS 25600
#define CAL_BLOCKS 100
#define CAL_HEADROOM 0.95f

typedef void (*iq_callback)(float *data, int n_floats, void *user);
typedef void (*log_callback)(const char *msg);

typedef struct {
    log_callback on_log;
    void *user;
    volatile int running;
    int power_level;
    char device_name[64];
} rf_context;

static void rf_log(rf_context *ctx, const char *msg) {
    fprintf(stderr, "[pluto_tx] %s\n", msg);
    if (ctx && ctx->on_log) ctx->on_log(msg);
}

static int run_pluto(rf_context *ctx, int gpssim_fd, int srv_fd) {
    (void)srv_fd;
    struct iio_context *iioc = NULL;
    
    iioc = iio_create_context_from_uri("usb:1.2.5");
    if (!iioc) iioc = iio_create_context_from_uri("ip:192.168.2.1");
    if (!iioc) iioc = iio_create_context_from_uri("ip:pluto.local");
    if (!iioc) {
        rf_log(ctx, "Cannot connect to PlutoSDR");
        return 1;
    }
    iio_context_set_timeout(iioc, 10000);

    struct iio_device *phy = iio_context_find_device(iioc, "ad9361-phy");
    struct iio_device *txdev = iio_context_find_device(iioc, "cf-ad9361-dds-core-lpc");
    if (!phy || !txdev) {
        rf_log(ctx, "PlutoSDR devices not found");
        iio_context_destroy(iioc);
        return 1;
    }

    iio_device_set_kernel_buffers_count(txdev, 8);

    struct iio_channel *phy_chn = iio_device_find_channel(phy, "voltage0", 1);
    if (!phy_chn) { iio_context_destroy(iioc); return 1; }

    iio_channel_attr_write(phy_chn, "rf_port_select", "A");
    iio_channel_attr_write_longlong(phy_chn, "rf_bandwidth", 6000000LL);
    iio_channel_attr_write_longlong(phy_chn, "sampling_frequency", 2560000LL);

    int patten = -40 + (ctx->power_level > 10 ? 10 : ctx->power_level) * 4;
    if (patten > 0) patten = 0; if (patten < -40) patten = -40;
    iio_channel_attr_write_longlong(phy_chn, "hardwaregain", (long long)patten);

    struct iio_channel *rxlo = iio_device_find_channel(phy, "altvoltage0", 1);
    if (rxlo) iio_channel_attr_write(rxlo, "powerdown", "1");

    struct iio_channel *txlo = iio_device_find_channel(phy, "altvoltage1", 1);
    if (txlo) {
        iio_channel_attr_write_longlong(txlo, "frequency", 1575420000LL);
        iio_channel_attr_write(txlo, "powerdown", "0");
    }

    struct iio_channel *tx0 = iio_device_find_channel(txdev, "voltage0", 1);
    if (!tx0) tx0 = iio_device_find_channel(txdev, "altvoltage0", 1);
    struct iio_channel *tx1 = iio_device_find_channel(txdev, "voltage1", 1);
    if (!tx1) tx1 = iio_device_find_channel(txdev, "altvoltage1", 1);
    if (!tx0 || !tx1) { iio_context_destroy(iioc); return 1; }
    iio_channel_enable(tx0);
    iio_channel_enable(tx1);

    usleep(200000);

    int buf_samps = 262144;
    struct iio_buffer *txbuf = iio_device_create_buffer(txdev, buf_samps, 0);
    if (!txbuf) { buf_samps = 131072; txbuf = iio_device_create_buffer(txdev, buf_samps, 0); }
    if (!txbuf) { buf_samps = 65536; txbuf = iio_device_create_buffer(txdev, buf_samps, 0); }
    if (!txbuf) {
        rf_log(ctx, "Failed to create IIO buffer");
        iio_context_destroy(iioc);
        return 1;
    }

    if (txlo) iio_channel_attr_write(txlo, "powerdown", "0");
    rf_log(ctx, "PlutoSDR TX started (1575.42MHz, 2.6Msps)");

    float *iq = (float *)malloc(BUF_SAMPS * 2 * sizeof(float));
    int total = BUF_SAMPS * 2 * (int)sizeof(float);
    int16_t *iq16 = (int16_t *)malloc(buf_samps * 2 * sizeof(int16_t));
    int buf_bytes = buf_samps * 2 * sizeof(int16_t);
    int buf_pos = 0;
    float cal_peak = 1e-6f;
    int cal_count = 0;

    while (ctx->running) {
        int got = 0, disconnected = 0;
        while (got < total && ctx->running) {
            int r = (int)recv(gpssim_fd, ((char *)iq) + got, total - got, 0);
            if (r > 0) { got += r; continue; }
            if (r == 0) { disconnected = 1; break; }
            if (errno == EAGAIN || errno == EINTR) { if (!ctx->running) break; continue; }
            disconnected = 1; break;
        }
        if (!ctx->running || disconnected) break;

        float maxVal = 1e-6f;
        for (int ni = 0; ni < BUF_SAMPS * 2; ni++) { float a = fabsf(iq[ni]); if (a > maxVal) maxVal = a; }
        if (cal_count < CAL_BLOCKS) {
            if (maxVal > cal_peak) cal_peak = maxVal;
            cal_count++;
        }
        float scale = (cal_count >= CAL_BLOCKS) ? (CAL_HEADROOM / cal_peak) : (1.0f / maxVal);
        for (int i = 0; i < BUF_SAMPS * 2; i++) {
            float v = iq[i] * scale; iq[i] = v;
            int16_t s = (int16_t)(v * 32767.0f);
            iq16[buf_pos++] = s;
            if (buf_pos >= buf_samps * 2) {
                void *ptr = iio_buffer_start(txbuf);
                memcpy(ptr, iq16, buf_bytes);
                iio_buffer_push(txbuf);
                buf_pos = 0;
            }
        }
    }

    if (buf_pos > 0) {
        memset(iq16 + buf_pos, 0, (buf_samps * 2 - buf_pos) * sizeof(int16_t));
        void *ptr = iio_buffer_start(txbuf);
        memcpy(ptr, iq16, buf_bytes);
        iio_buffer_push(txbuf);
    }

    free(iq16); free(iq);
    iio_buffer_destroy(txbuf);
    iio_context_destroy(iioc);
    rf_log(ctx, "PlutoSDR stopped");
    return 0;
}

int main(int argc, char *argv[]) {
    rf_context ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.running = 1;
    ctx.power_level = 5;
    strcpy(ctx.device_name, "pluto");

    int port = 1999;
    int opt;
    while ((opt = getopt(argc, argv, "p:g:")) != -1) {
        switch (opt) {
            case 'p': port = atoi(optarg); break;
            case 'g': ctx.power_level = atoi(optarg); break;
        }
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int optval = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    int rcvbuf = 262144;
    setsockopt(srv, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = INADDR_ANY;
    if (bind(srv, (struct sockaddr *)&a, sizeof(a)) < 0) {
        rf_log(NULL, "bind failed");
        return 1;
    }
    listen(srv, 1);
    rf_log(NULL, "Waiting for gpssim connection...");

    while (ctx.running) {
        int gpssim_fd = -1;
        while (ctx.running) {
            fd_set rfds; FD_ZERO(&rfds); FD_SET(srv, &rfds);
            struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 500000;
            int sr = select(srv + 1, &rfds, NULL, NULL, &tv);
            if (sr > 0 && FD_ISSET(srv, &rfds)) { gpssim_fd = accept(srv, NULL, NULL); break; }
        }
        if (!ctx.running) break;
        rf_log(NULL, "gpssim connected");

        struct timeval rcv_tv; rcv_tv.tv_sec = 0; rcv_tv.tv_usec = 500000;
        setsockopt(gpssim_fd, SOL_SOCKET, SO_RCVTIMEO, &rcv_tv, sizeof(rcv_tv));

        int ret = run_pluto(&ctx, gpssim_fd, srv);
        close(gpssim_fd);
        if (ret != 0) { rf_log(NULL, "PlutoSDR failed, stopping"); break; }
        if (!ctx.running) break;
        rf_log(NULL, "gpssim disconnected, waiting for reconnect...");
    }
    close(srv);
    return 0;
}
