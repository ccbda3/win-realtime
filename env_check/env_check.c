#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <shlobj.h>
#include <winternl.h>
#include <tlhelp32.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

static FILE *g_rep = NULL;

static char *w2u(const wchar_t *ws) {
    int n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, NULL, 0, NULL, NULL);
    if (n <= 0) return _strdup("");
    char *s = (char*)malloc(n);
    if (!s) return _strdup("");
    WideCharToMultiByte(CP_UTF8, 0, ws, -1, s, n, NULL, NULL);
    return s;
}

static void w(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (g_rep) vfprintf(g_rep, fmt, ap);
    va_end(ap);
}

static void section(const char *title) {
    w("\n========== %s ==========\n", title);
}

static int ping_icmp(const char *ip);

static void get_desktop(wchar_t *buf, DWORD len) {
    if (SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, buf) != S_OK)
        buf[0] = 0;
}

static void get_username(wchar_t *buf, DWORD len) {
    DWORD n = len;
    if (!GetUserNameW(buf, &n)) buf[0] = 0;
}

static void get_computername(wchar_t *buf, DWORD len) {
    DWORD n = len;
    if (!GetComputerNameW(buf, &n)) buf[0] = 0;
}

static void detect_system(void) {
    section("系统信息");
    wchar_t name[128] = {0}, user[128] = {0};
    get_computername(name, 128);
    get_username(user, 128);
    char *u;
    u = w2u(name); w("计算机名: %s\n", u); free(u);
    u = w2u(user); w("用户名: %s\n", u); free(u);

    OSVERSIONINFOEXW ovi;
    ZeroMemory(&ovi, sizeof(ovi));
    ovi.dwOSVersionInfoSize = sizeof(ovi);
    typedef LONG (WINAPI *RtlGetVersion_t)(PRTL_OSVERSIONINFOW);
    HMODULE hNt = GetModuleHandleW(L"ntdll.dll");
    if (hNt) {
        RtlGetVersion_t f = (RtlGetVersion_t)GetProcAddress(hNt, "RtlGetVersion");
        if (f) f((PRTL_OSVERSIONINFOW)&ovi);
    }
    u = w2u(ovi.szCSDVersion);
    w("系统版本: %d.%d.%d (Build %d) %s\n", ovi.dwMajorVersion, ovi.dwMinorVersion, ovi.dwBuildNumber, ovi.dwBuildNumber, u);
    free(u);
    DWORD acp = GetACP();
    w("系统代码页(ACP): %u\n", acp);
    LCID lcid = GetUserDefaultLCID();
    w("用户区域(LCID): 0x%08X (%u)\n", lcid, lcid);

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    w("CPU 核数: %u\n", si.dwNumberOfProcessors);

    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    w("物理内存总量: %.1f GB (%.0f MB)\n", (double)ms.ullTotalPhys/1073741824.0, (double)ms.ullTotalPhys/1048576.0);
    w("可用内存: %.1f GB\n", (double)ms.ullAvailPhys/1073741824.0);
}

static void detect_time(void) {
    section("时间与时区");
    SYSTEMTIME lt, ut;
    GetLocalTime(&lt);
    GetSystemTime(&ut);
    TIME_ZONE_INFORMATION tz;
    DWORD r = GetTimeZoneInformation(&tz);
    w("本地时间: %04u-%02u-%02u %02u:%02u:%02u\n", lt.wYear, lt.wMonth, lt.wDay, lt.wHour, lt.wMinute, lt.wSecond);
    w("UTC 时间: %04u-%02u-%02u %02u:%02u:%02u\n", ut.wYear, ut.wMonth, ut.wDay, ut.wHour, ut.wMinute, ut.wSecond);
    w("时区偏移(分钟): %d\n", -(int)tz.Bias);
    w("时区状态: ");
    if (r == TIME_ZONE_ID_DAYLIGHT) w("夏令时\n");
    else if (r == TIME_ZONE_ID_STANDARD) w("标准时间\n");
    else w("未知(无夏令时)\n");
    char *u = w2u(tz.StandardName);
    w("时区名称: %s\n", u);
    free(u);
    time_t t = time(NULL);
    struct tm *tmv = localtime(&t);
    if (tmv) w("CRT本地时间: %s", asctime(tmv));
    w("Unix时间戳: %lld\n", (long long)t);
}

static void scan_settings(void) {
    section("settings.ini 检测(默认打开位置来源)");
    const wchar_t *cands[] = {
        L"%APPDATA%/PosNavTech/settings.ini",
        L"%APPDATA%/gps-ui-qt/PosNavTech/settings.ini",
        L"%LOCALAPPDATA%/PosNavTech/settings.ini",
        L"%LOCALAPPDATA%/gps-ui-qt/PosNavTech/settings.ini"
    };
    wchar_t tmp[MAX_PATH];
    int found = 0;
    char *u;
    for (int i = 0; i < 4; i++) {
        if (ExpandEnvironmentStringsW(cands[i], tmp, MAX_PATH) == 0) continue;
        if (GetFileAttributesW(tmp) == INVALID_FILE_ATTRIBUTES) continue;
        found = 1;
        u = w2u(tmp);
        w("找到: %s\n", u);
        free(u);
        FILE *f = _wfopen(tmp, L"rb");
        if (f) {
            char line[512];
            while (fgets(line, sizeof(line), f)) {
                line[strcspn(line, "\r\n")] = 0;
                w("  %s\n", line);
            }
            fclose(f);
        } else {
            w("  (无法读取文件)\n");
        }
    }
    if (!found) {
        w("未找到 settings.ini (候选位置已扫描 4 处)\n");
    }
    wchar_t base[MAX_PATH];
    if (ExpandEnvironmentStringsW(L"%APPDATA%", base, MAX_PATH)) {
        u = w2u(base);
        w("\n%%APPDATA%% = %s\n", u);
        free(u);
    }
    if (ExpandEnvironmentStringsW(L"%LOCALAPPDATA%", base, MAX_PATH)) {
        u = w2u(base);
        w("%%LOCALAPPDATA%% = %s\n", u);
        free(u);
    }
    if (ExpandEnvironmentStringsW(L"%USERPROFILE%", base, MAX_PATH)) {
        u = w2u(base);
        w("%%USERPROFILE%% = %s\n", u);
        free(u);
    }
}

static void detect_qtmap(wchar_t *dir) {
    wchar_t qp[MAX_PATH];
    swprintf(qp, MAX_PATH, L"%ls\\static\\qtmap.html", dir);
    char *u;
    if (GetFileAttributesW(qp) == INVALID_FILE_ATTRIBUTES) {
        u = w2u(qp);
        w("  static/qtmap.html 不存在: %s\n", u);
        free(u);
        return;
    }
    u = w2u(qp);
    w("  static/qtmap.html 存在: %s\n", u);
    free(u);
    FILE *f = _wfopen(qp, L"rb");
    if (!f) { w("  (无法打开)\n"); return; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc(sz + 1);
    if (!buf) { fclose(f); return; }
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = 0;
    fclose(f);

    w("  文件大小: %ld 字节\n", sz);
    w("  含 pos归位修复(pos = originalPathLinear[0]): %s\n",
        strstr(buf, "pos = originalPathLinear[0]") ? "是" : "否");
    w("  含抖动基准重置(_jitBaseLat = window.lat): %s\n",
        strstr(buf, "window._jitBaseLat = window.lat") ? "是" : "否");
    w("  含 pathCompleted 状态机(var pathCompleted): %s\n",
        strstr(buf, "var pathCompleted = false") ? "是" : "否");
    w("  含移除自动启动(未找到自动启动路径跟随): %s\n",
        strstr(buf, "自动启动路径跟随") ? "否(仍存在自动启动)" : "是(已移除)");
    w("  抖动半径默认值 jitterRadiusMeters = %s\n",
        strstr(buf, "var jitterRadiusMeters = 2.0") ? "2.0" :
        (strstr(buf, "var jitterRadiusMeters = 400") ? "400" : "其他"));
    free(buf);
}

static void scan_software_dir(void) {
    section("软件目录检测");
    wchar_t exeDir[MAX_PATH];
    GetModuleFileNameW(NULL, exeDir, MAX_PATH);
    wchar_t *p = wcsrchr(exeDir, L'\\');
    if (p) *p = 0;
    char *u = w2u(exeDir);
    w("本程序所在目录: %s\n", u);
    free(u);

    wchar_t dirs[4][MAX_PATH];
    swprintf(dirs[0], MAX_PATH, L"%ls", exeDir);
    swprintf(dirs[1], MAX_PATH, L"C:\\Users\\Whaoy\\Documents\\win-realtime\\gaoduan\\gps-ui-qt_portable");
    swprintf(dirs[2], MAX_PATH, L"C:\\Users\\Whaoy\\Documents\\win-realtime\\gps-ui-qt\\gps-ui-qt_portable");
    swprintf(dirs[3], MAX_PATH, L"%ls\\gps-ui-qt_portable", exeDir);

    for (int i = 0; i < 4; i++) {
        wchar_t exe[MAX_PATH];
        swprintf(exe, MAX_PATH, L"%ls\\gps-ui-qt.exe", dirs[i]);
        if (GetFileAttributesW(exe) != INVALID_FILE_ATTRIBUTES) {
            u = w2u(dirs[i]);
            w("\n找到 gps-ui-qt.exe: %s\n", u);
            free(u);
            WIN32_FILE_ATTRIBUTE_DATA fd;
            if (GetFileAttributesExW(exe, GetFileExInfoStandard, &fd)) {
                FILETIME ft = fd.ftLastWriteTime;
                SYSTEMTIME st;
                FileTimeToSystemTime(&ft, &st);
                w("  gps-ui-qt.exe 时间: %04u-%02u-%02u %02u:%02u:%02u 大小: %u\n",
                    st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, fd.nFileSizeLow);
            }
            wchar_t core[MAX_PATH];
            swprintf(core, MAX_PATH, L"%ls\\gps-core.exe", dirs[i]);
            if (GetFileAttributesExW(core, GetFileExInfoStandard, &fd)) {
                SYSTEMTIME st;
                FileTimeToSystemTime(&fd.ftLastWriteTime, &st);
                w("  gps-core.exe 时间: %04u-%02u-%02u %02u:%02u:%02u 大小: %u\n",
                    st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, fd.nFileSizeLow);
            } else {
                w("  gps-core.exe 不存在!\n");
            }
            detect_qtmap(dirs[i]);
        }
    }
}

static void read_desktop_log(void) {
    section("桌面日志 日志.log");
    wchar_t desktop[MAX_PATH];
    get_desktop(desktop, MAX_PATH);
    wchar_t lp[MAX_PATH];
    swprintf(lp, MAX_PATH, L"%ls\\日志.log", desktop);
    char *u;
    if (GetFileAttributesW(lp) == INVALID_FILE_ATTRIBUTES) {
        u = w2u(lp);
        w("未找到: %s\n", u);
        free(u);
        return;
    }
    FILE *f = _wfopen(lp, L"rb");
    if (!f) {
        u = w2u(lp);
        w("无法打开: %s\n", u);
        free(u);
        return;
    }
    u = w2u(lp);
    w("路径: %s\n", u);
    free(u);
    char line[512];
    int n = 0;
    while (fgets(line, sizeof(line), f) && n < 200) {
        line[strcspn(line, "\r\n")] = 0;
        w("  %s\n", line);
        n++;
    }
    if (!feof(f)) w("  ...(日志过长,仅显示前200行)\n");
    fclose(f);
}

static void scan_ephemeris(void) {
    section("星历文件检测");
    wchar_t dirs[3][MAX_PATH];
    GetModuleFileNameW(NULL, dirs[0], MAX_PATH);
    wchar_t *p = wcsrchr(dirs[0], L'\\');
    if (p) *p = 0;
    swprintf(dirs[1], MAX_PATH, L"%ls\\手动星历", dirs[0]);
    wchar_t desk[MAX_PATH];
    get_desktop(desk, MAX_PATH);
    swprintf(dirs[2], MAX_PATH, L"%ls", desk);

    for (int d = 0; d < 3; d++) {
        char *u = w2u(dirs[d]);
        w("\n目录: %s\n", u);
        free(u);
        wchar_t pat[MAX_PATH];
        swprintf(pat, MAX_PATH, L"%ls\\*.*", dirs[d]);
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(pat, &fd);
        if (h == INVALID_HANDLE_VALUE) { w("  目录不存在或无文件\n"); continue; }
        int cnt = 0;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            int isEph = 0;
            if (wcsstr(fd.cFileName, L"brdc") || wcsstr(fd.cFileName, L"eph")) {
                isEph = 1;
            } else {
                wchar_t *dot = wcsrchr(fd.cFileName, L'.');
                if (dot) {
                    size_t elen = wcslen(dot + 1);
                    wchar_t *e = dot + 1;
                    if (elen >= 2 && elen <= 3 && e[0] >= L'0' && e[0] <= L'9') {
                        if (e[elen - 1] == L'n' || e[elen - 1] == L'g') isEph = 1;
                    }
                }
            }
            if (isEph) {
                u = w2u(fd.cFileName);
                w("  星历: %s 大小=%u\n", u, fd.nFileSizeLow);
                free(u);
                cnt++;
            }
        } while (FindNextFileW(h, &fd) && cnt < 50);
        FindClose(h);
        if (cnt == 0) w("  无星历文件\n");
    }
}

static void detect_network(void) {
    section("网络检测");
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    char host[256] = {0};
    if (gethostname(host, sizeof(host)) == 0) {
        w("主机名: %s\n", host);
        struct addrinfo hints, *res = NULL;
        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(host, NULL, &hints, &res) == 0) {
            for (struct addrinfo *r = res; r; r = r->ai_next) {
                struct sockaddr_in *sa = (struct sockaddr_in*)r->ai_addr;
                w("  本机IPv4: %s\n", inet_ntoa(sa->sin_addr));
            }
            freeaddrinfo(res);
        }
    }

    ULONG outBufLen = 15000;
    PIP_ADAPTER_INFO pAd = (PIP_ADAPTER_INFO)malloc(outBufLen);
    if (GetAdaptersInfo(pAd, &outBufLen) == ERROR_SUCCESS) {
        for (PIP_ADAPTER_INFO a = pAd; a; a = a->Next) {
            if (strlen((char*)a->IpAddressList.IpAddress.String) > 0) {
                w("适配器 %s: IP=%s 掩码=%s 网关=%s\n",
                    a->AdapterName, a->IpAddressList.IpAddress.String,
                    a->IpAddressList.IpMask.String, a->GatewayList.IpAddress.String);
            }
        }
    }
    free(pAd);

    const char *ports[] = {"1999", "5678", "8080", "8081", "8082", "8083", "8084", "8085"};
    for (int i = 0; i < 8; i++) {
        int pt = atoi(ports[i]);
        SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
        struct sockaddr_in sa;
        sa.sin_family = AF_INET;
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
        sa.sin_port = htons((u_short)pt);
        if (bind(s, (struct sockaddr*)&sa, sizeof(sa)) == 0) {
            w("端口 %s: 空闲(未被占用)\n", ports[i]);
        } else {
            w("端口 %s: 已被占用(Err=%d)\n", ports[i], WSAGetLastError());
        }
        closesocket(s);
    }

    SOCKET c = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in tgt;
    tgt.sin_family = AF_INET;
    tgt.sin_port = htons(1999);
    tgt.sin_addr.s_addr = inet_addr("127.0.0.1");
    unsigned long nb = 1;
    ioctlsocket(c, FIONBIO, &nb);
    int cr = connect(c, (struct sockaddr*)&tgt, sizeof(tgt));
    int connOk = 0;
    if (cr == 0) {
        connOk = 1;
    } else if (WSAGetLastError() == WSAEWOULDBLOCK) {
        fd_set wf;
        FD_ZERO(&wf);
        FD_SET(c, &wf);
        struct timeval tv = {0, 300000};
        if (select(0, NULL, &wf, NULL, &tv) > 0) {
            int err = 0, errlen = sizeof(err);
            getsockopt(c, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
            if (err == 0) connOk = 1;
        }
    }
    if (!connOk) {
        w("本机 127.0.0.1:1999 连接失败 —— gps-core/rf_core 未监听\n");
    } else {
        w("本机 127.0.0.1:1999 可连接(监听中)\n");
    }
    closesocket(c);

    int r = ping_icmp("127.0.0.1");
    w("ping 127.0.0.1: %s\n", r ? "成功" : "失败");
    r = ping_icmp("192.168.2.1");
    w("ping 192.168.2.1 (PlutoSDR): %s\n", r ? "成功" : "失败/无设备");
    WSACleanup();
}

static int ping_icmp(const char *ip) {
    HANDLE h = IcmpCreateFile();
    if (h == INVALID_HANDLE_VALUE) return 0;
    unsigned long addr = inet_addr(ip);
    char send[32] = {0};
    char reply[sizeof(ICMP_ECHO_REPLY) + 32];
    DWORD r = IcmpSendEcho(h, addr, send, 8, NULL, reply, sizeof(reply), 1000);
    IcmpCloseHandle(h);
    return r != 0;
}

static void detect_processes(void) {
    section("进程检测");
    const wchar_t *targets[] = { L"gps-ui-qt.exe", L"gps-core.exe" };
    int found[2] = {0};
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do {
                for (int i = 0; i < 2; i++) {
                    if (found[i]) continue;
                    if (_wcsicmp(pe.szExeFile, targets[i]) == 0) {
                        w("  %ls 正在运行 (PID=%lu)\n", targets[i], pe.th32ProcessID);
                        found[i] = 1;
                    }
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }
    for (int i = 0; i < 2; i++)
        if (!found[i]) w("  %ls 未运行\n", targets[i]);
}

static void detect_disk(void) {
    section("磁盘空间");
    wchar_t exeDir[MAX_PATH];
    GetModuleFileNameW(NULL, exeDir, MAX_PATH);
    wchar_t *p = wcsrchr(exeDir, L'\\');
    if (p) *p = 0;
    ULARGE_INTEGER freeB, totalB, totalFree;
    if (GetDiskFreeSpaceExW(exeDir, &freeB, &totalB, &totalFree)) {
        w("程序目录: %ls\n", exeDir);
        w("  总容量: %.1f GB  可用: %.1f GB\n",
            (double)totalB.QuadPart/1073741824.0, (double)freeB.QuadPart/1073741824.0);
    }
    if (GetDiskFreeSpaceExW(L"C:\\", &freeB, &totalB, &totalFree)) {
        w("C盘: 总容量: %.1f GB  可用: %.1f GB\n",
            (double)totalB.QuadPart/1073741824.0, (double)freeB.QuadPart/1073741824.0);
    }
}

static void detect_licenses(void) {
    section("授权文件检测");
    wchar_t exeDir[MAX_PATH];
    GetModuleFileNameW(NULL, exeDir, MAX_PATH);
    wchar_t *p = wcsrchr(exeDir, L'\\');
    if (p) *p = 0;
    wchar_t pat[MAX_PATH];
    swprintf(pat, MAX_PATH, L"%ls\\*.lic", exeDir);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        w("本目录无 .lic 文件\n");
        return;
    }
    do {
        w("  %ls 大小=%u\n", fd.cFileName, fd.nFileSizeLow);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void detect_gps_core_version(void) {
    section("gps-core 版本行为提示");
    w("若 gps-core 启动后出现 connect to 127.0.0.1:1999 failed (10061):\n");
    w("  说明 rf_core(gps-ui-qt.exe内) 未监听 1999, 常见原因:\n");
    w("  1. 上一次软件未完全退出, 旧进程占用/残留\n");
    w("  2. gps-core 启动早于 rf_core 监听就绪, 重试30次内未连上\n");
    w("  3. 杀毒软件拦截了本地回环 TCP 连接\n");
    w("建议: 完全退出软件后重开; 检查任务管理器无残留 gps-core.exe\n");
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    wchar_t desktop[MAX_PATH];
    get_desktop(desktop, MAX_PATH);

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t repPath[MAX_PATH];
    swprintf(repPath, MAX_PATH, L"%ls\\环境检测报告_%04u%02u%02u_%02u%02u%02u.txt",
        desktop, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    g_rep = _wfopen(repPath, L"wb");
    if (!g_rep) {
        wchar_t msg[512];
        swprintf(msg, 512, L"无法创建报告文件:\n%ls", repPath);
        MessageBoxW(NULL, msg, L"环境检测", MB_ICONERROR);
        return 1;
    }

    w("环境检测报告 - 生成时间 %04u-%02u-%02u %02u:%02u:%02u\n", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    detect_system();
    detect_time();
    scan_settings();
    scan_software_dir();
    read_desktop_log();
    scan_ephemeris();
    detect_network();
    detect_processes();
    detect_disk();
    detect_licenses();
    detect_gps_core_version();

    fclose(g_rep);
    g_rep = NULL;

    wchar_t msg[600];
    swprintf(msg, 600, L"环境检测完成!\n报告已保存到:\n%ls", repPath);
    MessageBoxW(NULL, msg, L"环境检测", MB_ICONINFORMATION | MB_OK);
    return 0;
}
