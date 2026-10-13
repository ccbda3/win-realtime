#define _GNU_SOURCE
#define BTSTACK_FILE__ "ble_daemon.c"
#include "btstack.h"
#include "btstack_run_loop_posix.h"
#include "btstack_tlv_posix.h"
#include "hci_dump_posix_fs.h"
#include "hci_transport_linux.h"
#include "ble/le_device_db_tlv.h"
#include "ble_gatt.h"
#include <signal.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <zlib.h>

#define EPH_PATH "/home/gps/gps-pi/brdc.22n"
#define EPH_MAX 204800
#define LOG_PATH "/tmp/ble.log"
#define STATE_PATH "/tmp/blec_state.json"
#define PID_PATH "/tmp/blec.pid"

static void dlog(const char *fmt, ...) {
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    FILE *f = fopen(LOG_PATH, "a");
    if (f) {
        fprintf(f, "[%02d:%02d:%02d] ", lt->tm_hour, lt->tm_min, lt->tm_sec);
        va_list ap;
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);
        fprintf(f, "\n");
        fclose(f);
    }
}

static hci_con_handle_t con_handle = HCI_CON_HANDLE_INVALID;
static int le_notification_enabled = 0;
static char connected_mac[18] = "";
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

static uint8_t eph_buf[EPH_MAX];
static int eph_len = 0;
static int eph_expected_len = 0;
static int eph_total_pkts = 0;
static int eph_seq = 1;
static int eph_first_seq = -1;
static int eph_chunk = 54;
static int eph_active = 0;
static int eph_saved = 0;
static uint16_t eph_file_crc = 0;

static long stat_writes = 0;
static long stat_handshakes = 0;
static long stat_requests = 0;
static long stat_eph_pkts = 0;
static long stat_coords = 0;
static long stat_notifies = 0;
static long stat_disconnects = 0;
static time_t conn_start = 0;
static time_t daemon_start = 0;
static time_t tx_start = 0;
static char last_coord[128] = "";
static double last_lat = 0;
static double last_lon = 0;
static double last_alt = 10;

static struct btstack_timer_source coord_timer;
static void coord_timer_handler(struct btstack_timer_source *ts) {
    if (!last_coord[0]) return;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s >= 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(5678);
        addr.sin_addr.s_addr = htonl(0x7F000001);
        double data[3] = {last_lat, last_lon, last_alt};
        sendto(s, data, sizeof(data), 0, (struct sockaddr *)&addr, sizeof(addr));
        close(s);
        dlog("resend last coord after gpssim start");
    }
}

static int proc_running(const char *name) {
    DIR *d = opendir("/proc");
    if (!d) return 0;
    struct dirent *e;
    int found = 0;
    while ((e = readdir(d)) && !found) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        char path[64], comm[64], stat_buf[256];
        int zombie = 0;
        snprintf(path, sizeof(path), "/proc/%s/stat", e->d_name);
        FILE *f = fopen(path, "r");
        if (f) {
            if (fgets(stat_buf, sizeof(stat_buf), f)) {
                char *p = strrchr(stat_buf, ')');
                if (p && p[1] == ' Z') zombie = 1;
            }
            fclose(f);
        }
        if (zombie) continue;
        snprintf(path, sizeof(path), "/proc/%s/comm", e->d_name);
        f = fopen(path, "r");
        if (f) {
            if (fgets(comm, sizeof(comm), f)) {
                comm[strcspn(comm, "\n")] = 0;
                if (strcmp(comm, name) == 0) found = 1;
            }
            fclose(f);
        }
    }
    closedir(d);
    return found;
}

static void run_bg(char *const argv[], const char *logfile) {
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        int fd = open(logfile ? logfile : "/dev/null", O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd >= 0) {
            dup2(fd, 1);
            dup2(fd, 2);
            close(fd);
        }
        execv(argv[0], argv);
        _exit(1);
    }
}

static void start_tx(void) {
    if (proc_running("gpssim")) return;
    dlog("Starting gpssim + pluto_tx...");
    int lf = open("/tmp/gpssim.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (lf >= 0) close(lf);
    run_bg((char *[]){(char *)"/usr/bin/pkill", (char *)"-9", (char *)"-f", (char *)"pluto_tx", NULL}, NULL);
    run_bg((char *[]){(char *)"/usr/bin/pkill", (char *)"-9", (char *)"-f", (char *)"gpssim", NULL}, NULL);
    usleep(1000000);
    run_bg((char *[]){(char *)"/home/gps/gps-pi/pluto_tx", (char *)"-p", (char *)"1999", (char *)"-g", (char *)"7", NULL}, NULL);
    usleep(2000000);
    run_bg((char *[]){(char *)"/home/gps/gps-pi/gpssim", (char *)"-e", (char *)"/home/gps/gps-pi/brdc.22n",
        (char *)"-s", (char *)"2560000", (char *)"-n", (char *)"1999",
        (char *)"-d", (char *)"43200", (char *)"-w", (char *)"5678", (char *)"-b", (char *)"8", NULL},
        "/tmp/gpssim.log");
    tx_start = time(NULL);
    dlog("gpssim started");
    if (last_coord[0]) {
        coord_timer.process = &coord_timer_handler;
        btstack_run_loop_set_timer(&coord_timer, 3500);
        btstack_run_loop_add_timer(&coord_timer);
    }
}

static void stop_tx(void) {
    if (!proc_running("gpssim") && !proc_running("pluto_tx")) return;
    dlog("Stopping gpssim + pluto_tx...");
    run_bg((char *[]){(char *)"/usr/bin/pkill", (char *)"-9", (char *)"-f", (char *)"pluto_tx", NULL}, NULL);
    run_bg((char *[]){(char *)"/usr/bin/pkill", (char *)"-9", (char *)"-f", (char *)"gpssim", NULL}, NULL);
    int w = 0;
    while (w < 20 && (proc_running("gpssim") || proc_running("pluto_tx"))) {
        usleep(100000);
        w++;
    }
    tx_start = 0;
    dlog("gpssim stopped");
}

static int usb_pluto(void) {
    DIR *d = opendir("/sys/bus/usb/devices");
    if (!d) return 0;
    struct dirent *e;
    int found = 0;
    while ((e = readdir(d)) && !found) {
        char vp[128], pp[128];
        snprintf(vp, sizeof(vp), "/sys/bus/usb/devices/%s/idVendor", e->d_name);
        FILE *f = fopen(vp, "r");
        if (!f) continue;
        char v[16] = "";
        fgets(v, sizeof(v), f);
        fclose(f);
        v[strcspn(v, "\n")] = 0;
        if (strcmp(v, "0456") != 0) continue;
        snprintf(pp, sizeof(pp), "/sys/bus/usb/devices/%s/idProduct", e->d_name);
        f = fopen(pp, "r");
        if (!f) continue;
        char p[16] = "";
        fgets(p, sizeof(p), f);
        fclose(f);
        p[strcspn(p, "\n")] = 0;
        if (strcmp(p, "b673") == 0) found = 1;
    }
    closedir(d);
    return found;
}

static uint8_t sum8(const uint8_t *d, int n) {
    uint8_t s = 0;
    int i;
    for (i = 0; i < n; i++) s += d[i];
    return s;
}

static uint16_t crc16_xmodem(const uint8_t *d, int n) {
    uint16_t crc = 0;
    int i, j;
    for (i = 0; i < n; i++) {
        crc ^= (uint16_t)d[i] << 8;
        for (j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

static void eph_reset(void) {
    eph_len = 0;
    eph_expected_len = 0;
    eph_file_crc = 0;
    eph_seq = 1;
    eph_first_seq = -1;
    eph_active = 0;
    eph_saved = 0;
    eph_total_pkts = 0;
}

static void send_report(uint8_t cmd, const uint8_t *payload, int plen) {
    uint8_t frame[16];
    memset(frame, 0, sizeof(frame));
    frame[0] = cmd;
    if (plen > 14) plen = 14;
    memcpy(frame + 1, payload, plen);
    frame[15] = sum8(frame + 1, 14);
    stat_notifies++;
    if (con_handle != HCI_CON_HANDLE_INVALID && le_notification_enabled) {
        int rc = att_server_notify(con_handle, ATT_CHARACTERISTIC_0000FF01_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, frame, 16);
        if (rc) dlog("notify fail cmd=0x%02x rc=%d", cmd, rc);
    }
    dlog("report cmd=0x%02x len=16", cmd);
}

static int eph_save(void) {
    uint8_t *bin = eph_buf;
    int bin_len = eph_len;
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK) return 0;
    uint8_t out[524288];
    zs.next_in = bin;
    zs.avail_in = (uInt)bin_len;
    zs.next_out = out;
    zs.avail_out = sizeof(out);
    int r = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    if (r != Z_STREAM_END) {
        dlog("eph_save: gzip error %d", r);
        return 0;
    }
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s.new", EPH_PATH);
    FILE *f = fopen(tmp, "wb");
    if (!f) return 0;
    fwrite(out, 1, zs.total_out, f);
    fclose(f);
    rename(tmp, EPH_PATH);
    chmod(EPH_PATH, 0666);
    DIR *d = opendir("/home/gps/gps-pi");
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (strstr(e->d_name, ".22n") && strcmp(e->d_name, "brdc.22n") != 0) {
                char fp[256];
                snprintf(fp, sizeof(fp), "/home/gps/gps-pi/%s", e->d_name);
                unlink(fp);
                dlog("cleaned old eph: %s", fp);
            }
        }
        closedir(d);
    }
    dlog("Ephemeris saved: %lu bytes -> %s", zs.total_out, EPH_PATH);
    return 1;
}

static struct btstack_timer_source final_timer;
static uint8_t final_result = 0;

static void final_timer_handler(struct btstack_timer_source *ts) {
    dlog("final timer fired result=%d", final_result);
    send_report(0x03, &final_result, 1);
}

static void eph_finish(void) {
    dlog("Update finish: got %d bytes, expect %d", eph_len, eph_expected_len);
    if (eph_len == eph_expected_len && eph_save()) {
        uint16_t crc = crc16_xmodem(eph_buf, eph_len);
        dlog("Update done, crc16=0x%04x vs expect 0x%04x", crc, eph_file_crc);
        if (crc == eph_file_crc) {
            dlog("CRC16 match");
            eph_saved = 1;
            final_result = 0x00;
        } else {
            dlog("CRC16 mismatch");
            final_result = 0x01;
        }
    } else {
        dlog("Update finish failed");
        final_result = 0x01;
    }
    final_timer.process = &final_timer_handler;
    btstack_run_loop_set_timer(&final_timer, 300);
    btstack_run_loop_add_timer(&final_timer);
    eph_reset();
}

static int eph_handle_frame(const uint8_t *data, int len) {
    if (len < 4) return 0;
    uint8_t cmd = data[0];
    if (cmd == 0x04) {
        struct stat st;
        int exists = stat(EPH_PATH, &st) == 0;
        time_t t = exists ? st.st_mtime : time(NULL);
        struct tm *lt = localtime(&t);
        int y = lt->tm_year + 1900;
        uint8_t frame[8] = {0x04,
            ((y / 100) / 10) * 16 + (y / 100) % 10,
            ((y % 100) / 10) * 16 + (y % 100) % 10,
            ((lt->tm_mon + 1) / 10) * 16 + (lt->tm_mon + 1) % 10,
            (lt->tm_mday / 10) * 16 + lt->tm_mday % 10,
            (lt->tm_hour / 10) * 16 + lt->tm_hour % 10,
            (lt->tm_min / 10) * 16 + lt->tm_min % 10,
            0};
        frame[7] = sum8(frame + 1, 6);
        stat_notifies++;
        if (con_handle != HCI_CON_HANDLE_INVALID && le_notification_enabled) {
            int rc = att_server_notify(con_handle, ATT_CHARACTERISTIC_0000FF01_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, frame, 8);
            if (rc) dlog("notify fail cmd=0x04 rc=%d", rc);
        }
        dlog("file date reply: %02x%02x-%02x-%02x %02x:%02x len=8",
            frame[1], frame[2], frame[3], frame[4], frame[5], frame[6]);
        return 1;
    }
    int flen = data[1] | (data[2] << 8);
    if (len != flen + 3) {
        dlog("Frame length mismatch: got %d, header %d", len, flen + 3);
        return 0;
    }
    if (sum8(data + 1, len - 2) != data[len - 1]) {
        dlog("Frame checksum fail");
        if (cmd == 0x01) send_report(0x01, (uint8_t[]){0x01}, 1);
        else if (cmd == 0x02) send_report(0x02, (uint8_t[]){0x01, 0x00}, 2);
        return 1;
    }
    if (cmd == 0x01) {
        dlog("Update request received");
        int mtu = att_server_get_mtu(con_handle);
        if (mtu > 500) mtu = 500;
        if (mtu < 60) mtu = 60;
        eph_chunk = mtu - 6;
        dlog("Update request: hex_size=%d crc16=0x%04x mtu=%d chunk=%d",
            data[6] | (data[7] << 8) | (data[8] << 16), data[9] | (data[10] << 8), mtu, eph_chunk);
        eph_reset();
        eph_expected_len = data[6] | (data[7] << 8) | (data[8] << 16);
        eph_file_crc = data[9] | (data[10] << 8);
        eph_active = 1;
        eph_total_pkts = (eph_expected_len + eph_chunk - 1) / eph_chunk;
        uint8_t payload[7] = {0x00, 0x00, mtu & 0xFF, (mtu >> 8) & 0xFF,
            eph_total_pkts & 0xFF, (eph_total_pkts >> 8) & 0xFF, 0};
        send_report(0x01, payload, 7);
        return 1;
    }
    if (cmd == 0x02) {
        if (!eph_active) return 1;
        int seq = data[3] | (data[4] << 8);
        int clen = len - 6;
        if (clen > eph_chunk) {
            dlog("Code chunk too long: %d", clen);
            return 1;
        }
        if (eph_first_seq < 0) eph_first_seq = seq;
        if (seq != eph_first_seq + (eph_seq - 1)) {
            dlog("Code frame seq mismatch: got %d, want %d", seq, eph_first_seq + (eph_seq - 1));
            send_report(0x02, (uint8_t[]){0x00, 0x02}, 2);
            return 1;
        }
        if (eph_len + clen > EPH_MAX) return 1;
        memcpy(eph_buf + eph_len, data + 5, clen);
        eph_len += clen;
        eph_seq++;
        send_report(0x02, (uint8_t[]){0x00, 0x00}, 2);
        if (eph_len >= eph_expected_len) eph_finish();
        return 1;
    }
    if (cmd == 0x03) {
        if (eph_active) eph_finish();
        return 1;
    }
    return 0;
}

static void forward_coords(const uint8_t *text, int len) {
    char buf[512];
    int n = len < 511 ? len : 511;
    memcpy(buf, text, n);
    buf[n] = 0;
    if (!strstr(buf, "GGA")) return;
    char *part = strtok(buf, "&");
    while (part) {
        if (strstr(part, "GGA")) {
            char *p[20];
            int i = 0;
            p[0] = strtok(part, ",");
            while (p[i] && i < 19) p[++i] = strtok(NULL, ",");
            if (i < 10) { part = strtok(NULL, "&"); continue; }
            double lr = atof(p[2]);
            double lor = atof(p[4]);
            double lat = (int)(lr / 100) + (lr - (int)(lr / 100) * 100) / 60.0;
            double lon = (int)(lor / 100) + (lor - (int)(lor / 100) * 100) / 60.0;
            if (p[3][0] == 'S') lat = -lat;
            if (p[5][0] == 'W') lon = -lon;
            double alt = p[9][0] ? atof(p[9]) : 10.0;
            dlog("Forward to gpssim: lat=%.6f lon=%.6f alt=%.1f", lat, lon, alt);
            stat_coords++;
            last_lat = lat;
            last_lon = lon;
            last_alt = alt;
            snprintf(last_coord, sizeof(last_coord), "lat=%.6f lon=%.6f alt=%.1f", lat, lon, alt);
            int s = socket(AF_INET, SOCK_DGRAM, 0);
            if (s >= 0) {
                struct sockaddr_in addr;
                memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_port = htons(5678);
                addr.sin_addr.s_addr = htonl(0x7F000001);
                double data[3] = {lat, lon, alt};
                sendto(s, data, sizeof(data), 0, (struct sockaddr *)&addr, sizeof(addr));
                close(s);
            }
            start_tx();
            if (!tx_start) tx_start = time(NULL);
            return;
        }
        part = strtok(NULL, "&");
    }
}

static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size) {
    UNUSED(connection_handle);
    if (att_handle == ATT_CHARACTERISTIC_0000FF01_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE) {
        const uint8_t v[] = {0x4F, 0x4B};
        return att_read_callback_handle_blob(v, sizeof(v), offset, buffer, buffer_size);
    }
    return 0;
}

static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size) {
    UNUSED(offset);
    if (transaction_mode == ATT_TRANSACTION_MODE_ACTIVE) return 0;
    if (att_handle == ATT_CHARACTERISTIC_0000FF01_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE) {
        le_notification_enabled = little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
        con_handle = connection_handle;
        return 0;
    }
    if (att_handle == ATT_CHARACTERISTIC_0000FF02_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE) {
        stat_writes++;
        if (buffer_size == 6 && buffer[0] == 0x24) {
            stat_handshakes++;
            return 0;
        }
        if (buffer_size >= 2 && (buffer[0] == 0x01 || buffer[0] == 0x02 || buffer[0] == 0x03 || buffer[0] == 0x04)) {
            if (buffer[0] == 0x01) stat_requests++;
            else if (buffer[0] == 0x02) stat_eph_pkts++;
            if (eph_handle_frame(buffer, buffer_size)) return 0;
        }
        char text[512];
        int n = buffer_size < 511 ? buffer_size : 511;
        memcpy(text, buffer, n);
        text[n] = 0;
        if (n > 80) {
            char tmp[96];
            memcpy(tmp, text, 80);
            tmp[80] = 0;
            dlog("Write FF02: '%s...' (%d bytes)", tmp, buffer_size);
        } else {
            dlog("Write FF02: '%s' (%d bytes)", text, buffer_size);
        }
        forward_coords((uint8_t *)text, n);
    }
    return 0;
}

static btstack_tlv_posix_t tlv_context;
static const btstack_tlv_t *tlv_impl;

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);
    if (packet_type != HCI_EVENT_PACKET) return;
    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                bd_addr_t local;
                gap_local_bd_addr(local);
                char tlv_path[128];
                snprintf(tlv_path, sizeof(tlv_path), "/tmp/btstack_%s.tlv", bd_addr_to_str_with_delimiter(local, '-'));
                tlv_impl = btstack_tlv_posix_init_instance(&tlv_context, tlv_path);
                btstack_tlv_set_instance(tlv_impl, &tlv_context);
                le_device_db_tlv_configure(tlv_impl, &tlv_context);
                dlog("MV999 ready (%s)", bd_addr_to_str(local));
            }
            break;
        case HCI_EVENT_LE_META:
            if (hci_event_le_meta_get_subevent_code(packet) == HCI_SUBEVENT_LE_CONNECTION_COMPLETE) {
                con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                bd_addr_t addr;
                hci_subevent_le_connection_complete_get_peer_address(packet, addr);
                btstack_strcpy(connected_mac, sizeof(connected_mac), bd_addr_to_str(addr));
                conn_start = time(NULL);
                dlog("CONNECTED: %s handle=%d", connected_mac, con_handle);
                dlog("ATT MTU: %d", att_server_get_mtu(con_handle));
            }
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            stat_disconnects++;
            stop_tx();
            dlog("DISCONNECTED");
            con_handle = HCI_CON_HANDLE_INVALID;
            le_notification_enabled = 0;
            connected_mac[0] = 0;
            conn_start = 0;
            eph_reset();
            break;
        case SM_EVENT_JUST_WORKS_REQUEST:
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        default:
            break;
    }
}

static void reset_stats(void) {
    stat_writes = 0;
    stat_handshakes = 0;
    stat_requests = 0;
    stat_eph_pkts = 0;
    stat_coords = 0;
    stat_notifies = 0;
    stat_disconnects = 0;
    last_coord[0] = 0;
    eph_reset();
    dlog("stats reset");
}

static struct btstack_timer_source adv_timer;
static void adv_timer_handler(struct btstack_timer_source *ts) {
    gap_advertisements_enable(1);
    btstack_run_loop_set_timer(ts, 30000);
    btstack_run_loop_add_timer(ts);
}

static struct btstack_timer_source state_timer;
static void state_timer_handler(struct btstack_timer_source *ts) {
    if (access("/tmp/blec_reset", F_OK) == 0) {
        unlink("/tmp/blec_reset");
        reset_stats();
    }
    if (access("/tmp/blec_tx_on", F_OK) == 0) {
        unlink("/tmp/blec_tx_on");
        dlog("manual tx start");
        start_tx();
        if (!tx_start) tx_start = time(NULL);
    }
    if (access("/tmp/blec_tx_off", F_OK) == 0) {
        unlink("/tmp/blec_tx_off");
        dlog("manual tx stop");
        stop_tx();
    }
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    long conn_dur = conn_start ? (long)(time(NULL) - conn_start) : 0;
    long up = (long)(time(NULL) - daemon_start);
    int tx = proc_running("gpssim");
    int pluto = proc_running("pluto_tx");
    int usb = usb_pluto();
    long tx_dur = (tx && tx_start) ? (long)(time(NULL) - tx_start) : 0;
    FILE *f = fopen(STATE_PATH, "w");
    if (f) {
        fprintf(f,
            "{\"time\":\"%02d:%02d:%02d\",\"connected\":\"%s\",\"powered\":true,"
            "\"conn_dur\":%ld,\"uptime\":%ld,\"disconnects\":%ld,"
            "\"writes\":%ld,\"handshakes\":%ld,\"requests\":%ld,\"eph_pkts\":%ld,"
            "\"coords\":%ld,\"notifies\":%ld,\"last_coord\":\"%s\","
            "\"tx\":%s,\"pluto\":%s,\"usb\":%s,\"tx_dur\":%ld,"
            "\"eph\":{\"active\":%s,\"expected_len\":%d,\"file_crc\":\"0x%04X\","
            "\"received\":%d,\"seq\":%d,\"total_pkts\":%d,\"saved\":%s}}",
            lt->tm_hour, lt->tm_min, lt->tm_sec, connected_mac,
            conn_dur, up, stat_disconnects,
            stat_writes, stat_handshakes, stat_requests, stat_eph_pkts,
            stat_coords, stat_notifies, last_coord,
            tx ? "true" : "false", pluto ? "true" : "false", usb ? "true" : "false", tx_dur,
            eph_active ? "true" : "false", eph_expected_len, eph_file_crc,
            eph_len, eph_seq, eph_total_pkts, eph_saved ? "true" : "false");
        fclose(f);
    }
    btstack_run_loop_set_timer(ts, 1000);
    btstack_run_loop_add_timer(ts);
}

static void on_sigint(int sig) {
    UNUSED(sig);
    dlog("SIGTERM received, shutting down");
    hci_power_control(HCI_POWER_OFF);
    exit(0);
}

static const hci_transport_config_linux_t transport_config = {
    .type = HCI_TRANSPORT_CONFIG_LINUX,
    .device_id = 0,
};

int btstack_main(int argc, const char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);
    daemon_start = time(NULL);
    l2cap_init();
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(0);
    att_server_init(profile_data, att_read_callback, att_write_callback);

    gap_set_local_name("MV999");

    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(0x0030, 0x0030, 0, 0, null_addr, 0x07, 0x00);
    static const uint8_t adv_data[] = {
        0x02, 0x01, 0x06,
        0x06, 0x09, 'M', 'V', '9', '9', '9',
        0x09, 0x04, 0x18, 0x00, 0x50, 0x00, 0x00, 0x00, 0x80, 0x0C
    };
    gap_advertisements_set_data(sizeof(adv_data), (uint8_t *)adv_data);
    gap_advertisements_enable(1);

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);
    att_server_register_packet_handler(packet_handler);

    adv_timer.process = &adv_timer_handler;
    btstack_run_loop_set_timer(&adv_timer, 30000);
    btstack_run_loop_add_timer(&adv_timer);

    state_timer.process = &state_timer_handler;
    btstack_run_loop_set_timer(&state_timer, 1000);
    btstack_run_loop_add_timer(&state_timer);

    hci_power_control(HCI_POWER_ON);
    return 0;
}

int main(int argc, const char *argv[]) {
    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);
    signal(SIGCHLD, SIG_IGN);
    FILE *log_init = fopen(LOG_PATH, "w");
    if (log_init) fclose(log_init);
    FILE *pf = fopen(PID_PATH, "w");
    if (pf) {
        fprintf(pf, "%d\n", (int)getpid());
        fclose(pf);
    }
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    unlink("/tmp/hci_dump.pklg");
    hci_dump_posix_fs_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);
    hci_dump_init(hci_dump_posix_fs_get_instance());
    const hci_transport_t *transport = hci_transport_linux_instance();
    hci_init(transport, (void *)&transport_config);
    btstack_main(argc, argv);
    btstack_run_loop_execute();
    return 0;
}
