#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>
#include <sys/stat.h>

#define STATE_PATH "/tmp/blec_state.json"
#define LOG_PATH "/tmp/ble.log"
#define GPSSIM_LOG_PATH "/tmp/gpssim.log"
#define EPH_FILE "/home/gps/gps-pi/brdc.22n"
#define RESET_PATH "/tmp/blec_reset"
#define TX_ON_PATH "/tmp/blec_tx_on"
#define TX_OFF_PATH "/tmp/blec_tx_off"

static void write_mark(const char *path) {
    FILE *f = fopen(path, "w");
    if (f) {
        fputc('1', f);
        fclose(f);
    }
}

static int read_file(const char *path, char *buf, int buf_size) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int n = (int)fread(buf, 1, buf_size - 1, f);
    fclose(f);
    buf[n] = 0;
    return n;
}

static int jstr(char *json, const char *key, char *out, int out_size) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    char *p = strstr(json, pat);
    if (!p) return 0;
    p += strlen(pat);
    char *e = strchr(p, '"');
    if (!e) return 0;
    int n = (int)(e - p);
    if (n >= out_size) n = out_size - 1;
    memcpy(out, p, n);
    out[n] = 0;
    return 1;
}

static int jint(char *json, const char *key) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    char *p = strstr(json, pat);
    if (!p) return 0;
    return atoi(p + strlen(pat));
}

static int jbool(char *json, const char *key) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    char *p = strstr(json, pat);
    if (!p) return 0;
    p += strlen(pat);
    return strncmp(p, "true", 4) == 0;
}

typedef struct {
    char time[32];
    char connected[32];
    int powered;
    int eph_active;
    int eph_expected;
    int eph_received;
    int eph_seq;
    int eph_total;
    char eph_crc[16];
    int eph_saved;
    char eph_date[24];
    int eph_size;
    int eph_sats;
    long writes, handshakes, requests, eph_pkts, coords, notifies, disconnects;
    long conn_dur, uptime;
    int tx, pluto, usb;
    long tx_dur;
    char last_coord[128];
    char log[128][512];
    int log_count;
    char glog[256][512];
    int glog_count;
} state_t;

static void parse_state(char *json, state_t *st) {
    memset(st, 0, sizeof(*st));
    jstr(json, "time", st->time, sizeof(st->time));
    jstr(json, "connected", st->connected, sizeof(st->connected));
    st->powered = jbool(json, "powered");
    st->conn_dur = jint(json, "conn_dur");
    st->uptime = jint(json, "uptime");
    st->disconnects = jint(json, "disconnects");
    st->writes = jint(json, "writes");
    st->handshakes = jint(json, "handshakes");
    st->requests = jint(json, "requests");
    st->eph_pkts = jint(json, "eph_pkts");
    st->coords = jint(json, "coords");
    st->notifies = jint(json, "notifies");
    st->tx = jbool(json, "tx");
    st->pluto = jbool(json, "pluto");
    st->usb = jbool(json, "usb");
    st->tx_dur = jint(json, "tx_dur");
    jstr(json, "last_coord", st->last_coord, sizeof(st->last_coord));
    char *s = strstr(json, "\"eph\":");
    if (s) {
        st->eph_active = jbool(s, "active");
        st->eph_expected = jint(s, "expected_len");
        st->eph_received = jint(s, "received");
        st->eph_seq = jint(s, "seq");
        st->eph_total = jint(s, "total_pkts");
        st->eph_saved = jbool(s, "saved");
        jstr(s, "file_crc", st->eph_crc, sizeof(st->eph_crc));
    }
}

static void load_log(state_t *st, int n) {
    char buf[65536];
    int got = read_file(LOG_PATH, buf, sizeof(buf));
    if (!got) return;
    char *save = NULL;
    char *tok = strtok_r(buf, "\n", &save);
    char lines[256][512];
    int count = 0;
    while (tok && count < 256) {
        snprintf(lines[count], sizeof(lines[count]), "%s", tok);
        count++;
        tok = strtok_r(NULL, "\n", &save);
    }
    int start = count > n ? count - n : 0;
    int i;
    for (i = start; i < count; i++) {
        snprintf(st->log[st->log_count], sizeof(st->log[0]), "%s", lines[i]);
        if (st->log_count >= 127) break;
        st->log_count++;
    }
}

static void load_gpssim_log(state_t *st, int n) {
    char buf[65536];
    int got = read_file(GPSSIM_LOG_PATH, buf, sizeof(buf));
    if (!got) {
        snprintf(st->glog[0], sizeof(st->glog[0]), "(gpssim 日志为空或不存在)");
        st->glog_count = 1;
        return;
    }
    char *save = NULL;
    char *tok = strtok_r(buf, "\n", &save);
    char lines[256][512];
    int count = 0;
    while (tok && count < 256) {
        snprintf(lines[count], sizeof(lines[count]), "%s", tok);
        count++;
        tok = strtok_r(NULL, "\n", &save);
    }
    int start = count > n ? count - n : 0;
    int i;
    for (i = start; i < count; i++) {
        snprintf(st->glog[st->glog_count], sizeof(st->glog[0]), "%s", lines[i]);
        if (st->glog_count >= 255) break;
        st->glog_count++;
    }
}

static void parse_eph_file(state_t *st) {
    st->eph_date[0] = 0;
    st->eph_size = 0;
    st->eph_sats = 0;
    struct stat fs;
    if (stat(EPH_FILE, &fs) != 0) return;
    st->eph_size = (int)fs.st_size;
    struct tm *lt = localtime(&fs.st_mtime);
    if (lt) snprintf(st->eph_date, sizeof(st->eph_date), "%04d-%02d-%02d %02d:%02d",
        lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min);
    FILE *f = fopen(EPH_FILE, "r");
    if (!f) return;
    char line[512];
    int in_header = 1;
    int seen[33] = {0};
    while (fgets(line, sizeof(line), f)) {
        if (in_header) {
            if (strncmp(line + 60, "END OF HEADER", 13) == 0) in_header = 0;
            continue;
        }
        char tmp[3];
        memcpy(tmp, line, 2);
        tmp[2] = 0;
        int sv = atoi(tmp);
        if (sv >= 1 && sv <= 32) seen[sv] = 1;
        int i;
        for (i = 0; i < 7; i++) {
            if (!fgets(line, sizeof(line), f)) break;
        }
    }
    fclose(f);
    int i;
    for (i = 1; i <= 32; i++) {
        if (seen[i]) st->eph_sats++;
    }
}

static void fmt_dur(long sec, char *out, int out_size) {
    if (sec < 60) {
        snprintf(out, out_size, "%ld秒", sec);
    } else if (sec < 3600) {
        snprintf(out, out_size, "%ld分%ld秒", sec / 60, sec % 60);
    } else {
        snprintf(out, out_size, "%ld时%ld分%ld秒", sec / 3600, (sec % 3600) / 60, sec % 60);
    }
}

static void print_bar(int filled, int total, int width) {
    int done = total > 0 ? (int)((long long)filled * width / total) : 0;
    int i;
    for (i = 0; i < width; i++) {
        putchar(i < done ? '#' : '.');
    }
    if (total > 0) {
        printf(" %d%%", (int)((long long)filled * 100 / total));
    }
}

static void render(state_t *st, int log_off, int rows, int cols) {
    char cd[64], up[64];
    fmt_dur(st->conn_dur, cd, sizeof(cd));
    fmt_dur(st->uptime, up, sizeof(up));
    printf("\x1b[H");
    printf("\x1b[1;36mMV999 BLE Monitor\x1b[0m  %s   \x1b[1;30m运行 %s\x1b[0m\n", st->time, up);
    printf("\x1b[1;33m连接:\x1b[0m %s   \x1b[1;33m已连接:\x1b[0m %s   \x1b[1;33m电源:\x1b[0m %s   \x1b[1;33m断开次数:\x1b[0m %ld\n",
        st->connected[0] ? st->connected : "\x1b[31m无连接\x1b[0m",
        st->connected[0] ? cd : "-",
        st->powered ? "\x1b[32mON\x1b[0m" : "\x1b[31mOFF\x1b[0m",
        st->disconnects);
    printf("\n\x1b[1;32m[ GPS 发射 ]\x1b[0m\n");
    if (st->tx) {
        char td[64];
        fmt_dur(st->tx_dur, td, sizeof(td));
        printf("  状态: \x1b[32m发射中\x1b[0m (已发射 %s)   发射器: %s\n",
            td, st->usb ? "\x1b[32m已连接\x1b[0m" : "\x1b[31m未连接\x1b[0m");
    } else {
        printf("  状态: \x1b[31m未发射\x1b[0m   发射器: %s\n",
            st->usb ? "\x1b[32m已连接\x1b[0m" : "\x1b[31m未连接\x1b[0m");
    }
    printf("\n\x1b[1;32m[ 星历传输 ]\x1b[0m\n");
    printf("  状态: %s  已收: %d / %d  包: %d / %d  CRC: %s  保存: %s\n",
        st->eph_active ? "\x1b[32m传输中\x1b[0m" : "\x1b[33m空闲\x1b[0m",
        st->eph_received, st->eph_expected, st->eph_seq, st->eph_total,
        st->eph_crc[0] ? st->eph_crc : "-",
        st->eph_saved ? "\x1b[32m是\x1b[0m" : "\x1b[33m否\x1b[0m");
    printf("  [");
    print_bar(st->eph_received, st->eph_expected, 40);
    printf("]\n");
    printf("  文件: %s  %d字节  %d颗卫星\n",
        st->eph_date[0] ? st->eph_date : "-", st->eph_size, st->eph_sats);
    printf("\n\x1b[1;32m[ 统计 ]\x1b[0m\n");
    printf("  收到写入: %ld   握手包: %ld   更新请求: %ld   代码包: %ld\n",
        st->writes, st->handshakes, st->requests, st->eph_pkts);
    printf("  发送通知: %ld   坐标转发: %ld\n", st->notifies, st->coords);
    printf("  最近坐标: %s\n", st->last_coord[0] ? st->last_coord : "-");
    printf("\n\x1b[1;32m[ 日志 ]\x1b[0m\n");
    int log_rows = rows - 19;
    if (log_rows < 4) log_rows = 4;
    int start = st->log_count - log_rows - log_off;
    if (start < 0) start = 0;
    int i;
    int lc = cols > 4 ? cols - 3 : 40;
    for (i = start; i < st->log_count && i < start + log_rows; i++) {
        char line[512];
        snprintf(line, sizeof(line), "%.*s", lc, st->log[i]);
        const char *l = line;
        if (strstr(l, "ERROR") || strstr(l, "fail") || strstr(l, "mismatch") || strstr(l, "error")) {
            printf("\x1b[31m  %s\x1b[0m\n", l);
        } else if (strstr(l, "CONNECTED") || strstr(l, "CRC16 match") || strstr(l, "Ephemeris saved")) {
            printf("\x1b[32m  %s\x1b[0m\n", l);
        } else if (strstr(l, "Update request") || strstr(l, "Forward")) {
            printf("\x1b[36m  %s\x1b[0m\n", l);
        } else {
            printf("  %s\n", l);
        }
    }
    while (i < start + log_rows) {
        printf("\n");
        i++;
    }
    printf("\n\x1b[1;30m[q]退出  [c]重置统计  [t]开发射  [s]停发射  [2]gpssim日志  [↑][↓]滚动日志\x1b[0m   \x1b[?25l");
    fflush(stdout);
}

static void render_gpssim(state_t *st, int log_off, int rows, int cols) {
    char td[64];
    fmt_dur(st->tx_dur, td, sizeof(td));
    printf("\x1b[H");
    printf("\x1b[1;36mMV999 GPSSIM 日志\x1b[0m  %s\n", st->time);
    if (st->tx) {
        printf("发射: \x1b[32m发射中\x1b[0m  时长: %s  发射器: %s\n",
            td, st->usb ? "\x1b[32m已连接\x1b[0m" : "\x1b[31m未连接\x1b[0m");
    } else {
        printf("发射: \x1b[31m未发射\x1b[0m  发射器: %s\n",
            st->usb ? "\x1b[32m已连接\x1b[0m" : "\x1b[31m未连接\x1b[0m");
    }
    printf("\x1b[1;32m[ gpssim 输出 ]\x1b[0m\n");
    int log_rows = rows - 7;
    if (log_rows < 4) log_rows = 4;
    int start = st->glog_count - log_rows - log_off;
    if (start < 0) start = 0;
    int i;
    int lc = cols > 4 ? cols - 3 : 40;
    for (i = start; i < st->glog_count && i < start + log_rows; i++) {
        char line[512];
        snprintf(line, sizeof(line), "%.*s", lc, st->glog[i]);
        const char *l = line;
        if (strstr(l, "error") || strstr(l, "fail")) {
            printf("\x1b[31m  %s\x1b[0m\n", l);
        } else {
            printf("  %s\n", l);
        }
    }
    while (i < start + log_rows) {
        printf("\n");
        i++;
    }
    printf("\n\x1b[1;30m[q]退出  [1]主页  [t]开发射  [s]停发射  [↑][↓]滚动\x1b[0m   \x1b[?25l");
    fflush(stdout);
}

static struct termios saved_tio;
static int log_off = 0;
static int page = 1;
static int last_page = 1;

static void term_raw(void) {
    struct termios tio;
    tcgetattr(0, &saved_tio);
    tio = saved_tio;
    tio.c_lflag &= ~(ICANON | ECHO);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &tio);
}

static void term_restore(void) {
    tcsetattr(0, TCSANOW, &saved_tio);
}

static int getch_timeout(int ms) {
    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    if (select(1, &fds, NULL, NULL, &tv) <= 0) return -1;
    unsigned char c;
    if (read(0, &c, 1) != 1) return -1;
    return c;
}

static void reset_daemon(void) {
    FILE *f = fopen(RESET_PATH, "w");
    if (f) {
        fputc('1', f);
        fclose(f);
    }
}

int main(int argc, char **argv) {
    int once = 0;
    if (argc > 1 && strcmp(argv[1], "--once") == 0) once = 1;
    int tty = isatty(1);
    if (!tty) once = 1;
    if (!once) {
        term_raw();
        printf("\x1b[?1049h\x1b[2J");
    }
    state_t st;
    for (;;) {
        char json[8192];
        if (read_file(STATE_PATH, json, sizeof(json))) {
            parse_state(json, &st);
        } else {
            memset(&st, 0, sizeof(st));
            snprintf(st.time, sizeof(st.time), "--");
            snprintf(st.log[0], sizeof(st.log[0]), "无法读取状态文件 (daemon未运行?)");
            st.log_count = 1;
        }
        load_log(&st, 128);
        load_gpssim_log(&st, 256);
        parse_eph_file(&st);
        if (once) {
            char cd[64], up[64];
            fmt_dur(st.conn_dur, cd, sizeof(cd));
            fmt_dur(st.uptime, up, sizeof(up));
            printf("MV999 Monitor  %s  运行 %s\n", st.time, up);
            printf("连接设备: %s  已连接: %s  电源: %s  断开次数: %ld\n",
                st.connected[0] ? st.connected : "无连接", st.connected[0] ? cd : "-",
                st.powered ? "ON" : "OFF", st.disconnects);
            if (st.tx) {
                char td[64];
                fmt_dur(st.tx_dur, td, sizeof(td));
                printf("GPS发射: 发射中 (已发射 %s)  发射器: %s\n",
                    td, st.usb ? "已连接" : "未连接");
            } else {
                printf("GPS发射: 未发射  发射器: %s\n", st.usb ? "已连接" : "未连接");
            }
            printf("星历: 状态=%s 已收=%d/%d 包=%d/%d CRC=%s 保存=%s\n",
                st.eph_active ? "传输中" : "空闲", st.eph_received, st.eph_expected,
                st.eph_seq, st.eph_total, st.eph_crc[0] ? st.eph_crc : "-",
                st.eph_saved ? "是" : "否");
            printf("星历文件: %s  %d字节  %d颗卫星\n",
                st.eph_date[0] ? st.eph_date : "-", st.eph_size, st.eph_sats);
            printf("统计: 写入=%ld 握手=%ld 请求=%ld 代码包=%ld 通知=%ld 坐标=%ld 断开=%ld\n",
                st.writes, st.handshakes, st.requests, st.eph_pkts, st.notifies, st.coords, st.disconnects);
            printf("最近坐标: %s\n", st.last_coord[0] ? st.last_coord : "-");
            printf("日志:\n");
            int i;
            for (i = 0; i < st.log_count; i++) {
                printf("  %s\n", st.log[i]);
            }
            break;
        }
        int rows = 24;
        int cols = 80;
        struct winsize ws;
        if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) rows = ws.ws_row;
        if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) cols = ws.ws_col;
        if (page != last_page) {
            printf("\x1b[2J");
            last_page = page;
        }
        if (page == 2) {
            render_gpssim(&st, log_off, rows, cols);
        } else {
            render(&st, log_off, rows, cols);
        }
        int ch = getch_timeout(1000);
        if (ch == 'q') break;
        if (ch == 'r') continue;
        if (ch == 'c') {
            reset_daemon();
            log_off = 0;
            continue;
        }
        if (ch == 't') {
            write_mark(TX_ON_PATH);
            continue;
        }
        if (ch == 's') {
            write_mark(TX_OFF_PATH);
            continue;
        }
        if (ch == '1') {
            page = 1;
            log_off = 0;
            continue;
        }
        if (ch == '2') {
            page = 2;
            log_off = 0;
            continue;
        }
        if (ch == 0x1b) {
            int c1 = getch_timeout(0);
            if (c1 == '[') {
                int c2 = getch_timeout(0);
                if (c2 == 'A' && log_off < 255) log_off++;
                else if (c2 == 'B' && log_off > 0) log_off--;
            }
        }
    }
    if (!once) {
        term_restore();
        printf("\x1b[0m\x1b[?25h\x1b[?1049l");
    }
    return 0;
}
