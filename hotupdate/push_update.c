#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "r2_credentials.h"

#pragma comment(lib, "shell32.lib")

typedef unsigned int u32;

static u32 md5k[64];
static int md5initdone = 0;

static void md5setup(void) {
    int i;
    for (i = 0; i < 64; i++) {
        double x = fabs(sin((double)(i + 1))) * 4294967296.0;
        md5k[i] = (u32)x;
    }
    md5initdone = 1;
}

static u32 md5shift(int i) {
    static const u32 sh[16] = {7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22};
    static const u32 sh2[16] = {5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20};
    static const u32 sh3[16] = {4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23};
    static const u32 sh4[16] = {6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
    if (i < 16) return sh[i];
    if (i < 32) return sh2[i - 16];
    if (i < 48) return sh3[i - 32];
    return sh4[i - 48];
}

static void md5_block(const unsigned char *p, u32 *h) {
    u32 a = h[0], b = h[1], c = h[2], d = h[3];
    u32 m[16];
    int i;
    for (i = 0; i < 16; i++) {
        m[i] = p[i*4] | (p[i*4+1] << 8) | (p[i*4+2] << 16) | ((u32)p[i*4+3] << 24);
    }
#define ROTL(x, s) (((x) << (s)) | ((x) >> (32 - (s))))
    for (i = 0; i < 64; i++) {
        u32 f, g;
        if (i < 16) { f = (b & c) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
        else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
        else { f = c ^ (b | ~d); g = (7 * i) % 16; }
        u32 tmp = d;
        d = c;
        c = b;
        b = b + ROTL(a + f + md5k[i] + m[g], md5shift(i));
        a = tmp;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
}

static void md5hex(const unsigned char *data, size_t len, char out[33]) {
    if (!md5initdone) md5setup();
    u32 h[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    size_t full = len / 64;
    size_t i;
    unsigned char pad[128];
    size_t rem, padlen;
    unsigned long long bits = (unsigned long long)len * 8;
    for (i = 0; i < full; i++) md5_block(data + i * 64, h);
    rem = len % 64;
    padlen = (rem < 56) ? 64 : 128;
    memset(pad, 0, sizeof(pad));
    memcpy(pad, data + full * 64, rem);
    pad[rem] = 0x80;
    for (i = 0; i < 8; i++) pad[padlen - 8 + i] = (unsigned char)(bits >> (8 * i));
    md5_block(pad, h);
    if (padlen > 64) md5_block(pad + 64, h);
    const char *hexd = "0123456789abcdef";
    int o = 0;
    int k;
    for (i = 0; i < 4; i++) {
        for (k = 0; k < 4; k++) {
            u32 byte = (h[i] >> (8 * k)) & 0xff;
            out[o++] = hexd[(byte >> 4) & 0xf];
            out[o++] = hexd[byte & 0xf];
        }
    }
    out[32] = 0;
}

static void getexedir(wchar_t *buf, int cap) {
    GetModuleFileNameW(NULL, buf, cap);
    wchar_t *s = wcsrchr(buf, L'\\');
    if (s) *s = 0;
}

static int readfile(const wchar_t *path, unsigned char **data, size_t *len) {
    FILE *f = _wfopen(path, L"rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = (unsigned char*)malloc(sz ? sz : 1);
    if (!buf) { fclose(f); return 0; }
    size_t rd = fread(buf, 1, sz, f);
    fclose(f);
    *data = buf;
    *len = rd;
    return 1;
}

static int run_curl(const wchar_t *args, wchar_t *errbuf, int errcap) {
    wchar_t curlpath[MAX_PATH];
    GetSystemDirectoryW(curlpath, MAX_PATH);
    wcscat(curlpath, L"\\curl.exe");
    if (GetFileAttributesW(curlpath) == INVALID_FILE_ATTRIBUTES) {
        wcsncpy(errbuf, L"系统未找到 curl.exe", errcap - 1);
        return -1;
    }
    wchar_t cmd[8192];
    swprintf(cmd, 8192, L"\"%ls\" %ls", curlpath, args);
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    wchar_t *mutable = _wcsdup(cmd);
    int ok = CreateProcessW(curlpath, mutable, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    free(mutable);
    if (!ok) {
        wcsncpy(errbuf, L"启动 curl 失败", errcap - 1);
        return -1;
    }
    DWORD waitRes = WaitForSingleObject(pi.hProcess, 60000);
    if (waitRes == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 5000);
        wcsncpy(errbuf, L"curl 超时已终止", errcap - 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return -2;
    }
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (code != 0) {
        swprintf(errbuf, errcap, L"curl 退出码 %lu", (unsigned long)code);
        return (int)code;
    }
    return 0;
}

static void urlenc(const char *in, char *out, int cap) {
    static const char *hexd = "0123456789ABCDEF";
    int o = 0;
    for (; *in && o < cap - 4; in++) {
        unsigned char c = (unsigned char)*in;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '/' || c == '~') {
            out[o++] = (char)c;
        } else {
            out[o++] = '%';
            out[o++] = hexd[c >> 4];
            out[o++] = hexd[c & 15];
        }
    }
    out[o] = 0;
}

struct upfile {
    wchar_t local[MAX_PATH];
    const char *key;
    int injson;
    int skipupload;
    char md5[33];
    int uploaded;
};

struct sfile {
    wchar_t local[MAX_PATH];
    char path[1024];
    char md5[33];
    int skipupload;
};

struct mline {
    char path[1024];
    char md5[33];
    int alive;
};

static void relslash(const wchar_t *full, const wchar_t *base, const char *prefix, char *out, int cap) {
    size_t blen = wcslen(base);
    const wchar_t *sub = full + blen;
    if (*sub == L'\\') sub++;
    char buf[1024];
    WideCharToMultiByte(CP_UTF8, 0, sub, -1, buf, 1024, NULL, NULL);
    for (char *p = buf; *p; p++) if (*p == '\\') *p = '/';
    if (prefix[0])
        _snprintf(out, cap, "%s/%s", prefix, buf);
    else
        _snprintf(out, cap, "%s", buf);
}

static int is_ignored(const char *rel) {
    const char *slash = strrchr(rel, '/');
    const char *name = slash ? slash + 1 : rel;
    if (name[0] == '.') return 1;
    if (strcmp(name, "version.txt") == 0) return 1;
    if (strcmp(name, "api_keys.json") == 0) return 1;
    size_t n = strlen(name);
    if (n > 4 && strcmp(name + n - 4, ".log") == 0) return 1;
    if (n > 4 && strcmp(name + n - 4, ".ini") == 0) return 1;
    if (n > 4 && strcmp(name + n - 4, ".tmp") == 0) return 1;
    if (n > 4 && strcmp(name + n - 4, ".dll") == 0) return 1;
    return 0;
}

static void scan_dir(const wchar_t *dir, const wchar_t *base, const char *prefix, struct sfile *list, int *nlist, int cap, int recurse) {
    if (*nlist >= cap) return;
    wchar_t pat[MAX_PATH];
    swprintf(pat, MAX_PATH, L"%ls\\*", dir);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            if (!recurse) continue;
            wchar_t sub[MAX_PATH];
            swprintf(sub, MAX_PATH, L"%ls\\%ls", dir, fd.cFileName);
            scan_dir(sub, base, prefix, list, nlist, cap, recurse);
        } else {
            if (*nlist >= cap) break;
            unsigned long long fsz = ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            if (fsz > 5ULL * 1024 * 1024) continue;
            wchar_t full[MAX_PATH];
            swprintf(full, MAX_PATH, L"%ls\\%ls", dir, fd.cFileName);
            char relbuf[1024];
            relslash(full, base, prefix, relbuf, 1024);
            if (is_ignored(relbuf)) continue;
            wcscpy(list[*nlist].local, full);
            strcpy(list[*nlist].path, relbuf);
            (*nlist)++;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

int main(void) {
    SetConsoleOutputCP(CP_UTF8);
    int silent = wcsstr(GetCommandLineW(), L"--silent") != NULL;
    wchar_t exedir[MAX_PATH];
    wchar_t root[MAX_PATH];
    getexedir(exedir, MAX_PATH);
    wcscpy(root, exedir);
    const char *accesskey = R2_ACCESS_KEY;
    const char *secret = R2_SECRET;
    const char *endpoint = R2_ENDPOINT;
    int isQuanneng = wcsstr(GetCommandLineW(), L"--channel=quanneng") != NULL;
    int force = wcsstr(GetCommandLineW(), L"--force") != NULL;
    const wchar_t *channelPath = isQuanneng ? L"update/quanneng/" : L"update/";

    char verstr[32] = "1.0.0";
    int verMaj = 1, verMin = 0, verPat = 0;
    wchar_t verfile[MAX_PATH];
    if (isQuanneng)
        swprintf(verfile, MAX_PATH, L"%ls\\push_version_quanneng.txt", root);
    else
        swprintf(verfile, MAX_PATH, L"%ls\\push_version.txt", root);
    FILE *vf = _wfopen(verfile, L"r");
    if (vf) {
        char vb[64] = {0};
        fgets(vb, sizeof(vb), vf);
        fclose(vf);
        {
            char *p = vb;
            if ((unsigned char)vb[0] == 0xEF && (unsigned char)vb[1] == 0xBB && (unsigned char)vb[2] == 0xBF) p += 3;
            if (strchr(p, '.'))
                sscanf(p, "%d.%d.%d", &verMaj, &verMin, &verPat);
            else
                verPat = atoi(p);
            sprintf(verstr, "%d.%d.%d", verMaj, verMin, verPat);
        }
    }

    wchar_t srcdir[MAX_PATH];
    if (isQuanneng)
        swprintf(srcdir, MAX_PATH, L"%ls\\gps-ui-qt\\gps-ui-qt_portable", root);
    else
        swprintf(srcdir, MAX_PATH, L"%ls\\gaoduan\\gps-ui-qt_portable", root);
    wchar_t updaterSrc[MAX_PATH];
    swprintf(updaterSrc, MAX_PATH, L"%ls\\hotupdate\\updater.exe", root);

    static struct upfile files[512];
    int nfiles = 0;
    wchar_t dels[64][MAX_PATH];
    int ndels = 0;
    memset(files, 0, sizeof(files));
    wchar_t tmp[MAX_PATH];
    wchar_t updDest[MAX_PATH];
    swprintf(updDest, MAX_PATH, L"%ls\\updater.exe", srcdir);
    CopyFileW(updaterSrc, updDest, FALSE);
    static struct sfile sfiles[512];
    int nsf = 0;
    scan_dir(srcdir, srcdir, "", sfiles, &nsf, 512, 0);
    wchar_t staticdir[MAX_PATH];
    swprintf(staticdir, MAX_PATH, L"%ls\\static", srcdir);
    if (GetFileAttributesW(staticdir) != INVALID_FILE_ATTRIBUTES)
        scan_dir(staticdir, staticdir, "static", sfiles, &nsf, 512, 1);
    const wchar_t *chanName = isQuanneng ? L"quanneng" : L"gaoduan";
    wchar_t manifestPath[MAX_PATH];
    swprintf(manifestPath, MAX_PATH, L"%ls\\stage\\manifest_%ls.txt", root, chanName);
    static struct mline oldm[512];
    int nold = 0;
    memset(oldm, 0, sizeof(oldm));
    {
        FILE *mf = _wfopen(manifestPath, L"r");
        if (mf) {
            char line[2048];
            while (nold < 512 && fgets(line, 2047, mf)) {
                char *nl = strchr(line, '\n'); if (nl) *nl = 0;
                char *cr = strchr(line, '\r'); if (cr) *cr = 0;
                if (!line[0]) continue;
                char *sep = strchr(line, '|');
                if (!sep) continue;
                *sep = 0;
                strncpy(oldm[nold].path, line, 1023);
                strncpy(oldm[nold].md5, sep + 1, 32);
                oldm[nold].md5[32] = 0;
                nold++;
            }
            fclose(mf);
        }
    }
    for (int i = 0; i < nsf; i++) {
        unsigned char *data;
        size_t len;
        if (!readfile(sfiles[i].local, &data, &len)) continue;
        md5hex(data, len, sfiles[i].md5);
        free(data);
        for (int j = 0; j < nold; j++) {
            if (strcmp(oldm[j].path, sfiles[i].path) == 0) {
                oldm[j].alive = 1;
                if (strcmp(oldm[j].md5, sfiles[i].md5) == 0) sfiles[i].skipupload = 1;
            }
        }
        if (nfiles >= 512) break;
        wcscpy(files[nfiles].local, sfiles[i].local);
        files[nfiles].key = (const char*)malloc(strlen(sfiles[i].path) + 1);
        strcpy((char*)files[nfiles].key, sfiles[i].path);
        strcpy(files[nfiles].md5, sfiles[i].md5);
        files[nfiles].injson = 1;
        files[nfiles].skipupload = sfiles[i].skipupload;
        files[nfiles].uploaded = sfiles[i].skipupload ? 1 : 0;
        nfiles++;
    }
    for (int i = 0; i < nold; i++) {
        if (oldm[i].alive) continue;
        if (is_ignored(oldm[i].path)) continue;
        if (ndels >= 64) break;
        for (char *p = oldm[i].path; *p; p++) if (*p == '\\') *p = '/';
        wchar_t wfull[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, oldm[i].path, -1, wfull, MAX_PATH);
        wchar_t wsrc[MAX_PATH];
        swprintf(wsrc, MAX_PATH, L"%ls\\%ls", srcdir, wfull);
        if (GetFileAttributesW(wsrc) != INVALID_FILE_ATTRIBUTES) continue;
        MultiByteToWideChar(CP_UTF8, 0, oldm[i].path, -1, dels[ndels], MAX_PATH);
        ndels++;
    }

    wchar_t extra[MAX_PATH];
    swprintf(extra, MAX_PATH, L"%ls\\stage\\extra_files.txt", root);
    char extraA[MAX_PATH];
    wcstombs(extraA, extra, MAX_PATH - 1);
    FILE *ef = fopen(extraA, "rb");
    if (ef) {
        char line[1024];
        while (nfiles < 128 && fgets(line, 1023, ef)) {
            char *nl = strchr(line, '\n'); if (nl) *nl = 0;
            char *cr = strchr(line, '\r'); if (cr) *cr = 0;
            if (!line[0]) continue;
            char *kp = (char*)malloc(strlen(line) + 1);
            strcpy(kp, line);
            for (char *p = kp; *p; p++) if (*p == '\\') *p = '/';
            wchar_t wpath[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, line, -1, wpath, MAX_PATH);
            swprintf(files[nfiles].local, MAX_PATH, L"%ls\\%ls", srcdir, wpath);
            files[nfiles].key = kp;
            files[nfiles].injson = 1;
            {
                unsigned char *edata;
                size_t elen;
                if (readfile(files[nfiles].local, &edata, &elen)) {
                    md5hex(edata, elen, files[nfiles].md5);
                    free(edata);
                }
                for (int j = 0; j < nold; j++) {
                    if (strcmp(oldm[j].path, kp) == 0 && strcmp(oldm[j].md5, files[nfiles].md5) == 0)
                        files[nfiles].skipupload = 1;
                }
            }
            files[nfiles].uploaded = files[nfiles].skipupload ? 1 : 0;
            nfiles++;
        }
        fclose(ef);
    }
    wchar_t removef[MAX_PATH];
    swprintf(removef, MAX_PATH, L"%ls\\stage\\remove_files.txt", root);
    char removefA[MAX_PATH];
    wcstombs(removefA, removef, MAX_PATH - 1);
    FILE *rf = fopen(removefA, "rb");
    if (rf) {
        char line[1024];
        while (ndels < 64 && fgets(line, 1023, rf)) {
            char *nl = strchr(line, '\n'); if (nl) *nl = 0;
            char *cr = strchr(line, '\r'); if (cr) *cr = 0;
            if (!line[0]) continue;
            for (char *p = line; *p; p++) if (*p == '\\') *p = '/';
            MultiByteToWideChar(CP_UTF8, 0, line, -1, dels[ndels], MAX_PATH);
            ndels++;
        }
        fclose(rf);
    }

    {
        int hasChange = 0;
        for (int i = 0; i < nfiles; i++)
            if (!files[i].skipupload && GetFileAttributesW(files[i].local) != INVALID_FILE_ATTRIBUTES) { hasChange = 1; break; }
        if (ndels > 0) hasChange = 1;
        if (!hasChange && !force) {
            wchar_t msg2[512];
            swprintf(msg2, 512, L"没有文件变化 (v%hs)，版本号未递增，未发布新版本。", verstr);
            if (silent) {
                wchar_t rp[MAX_PATH];
                swprintf(rp, MAX_PATH, L"%ls\\push_result.txt", root);
                FILE *rf = _wfopen(rp, L"w, ccs=UTF-8");
                if (rf) { fwprintf(rf, L"%ls", msg2); fclose(rf); }
            }
            char u8b[1600];
            WideCharToMultiByte(CP_UTF8, 0, msg2, -1, u8b, sizeof(u8b), NULL, NULL);
            printf("%s\n", u8b);
            return 0;
        }
        verPat++;
        if (verPat >= 100) { verPat = 0; verMin++; }
        if (verMin >= 100) { verMin = 0; verMaj++; }
        sprintf(verstr, "%d.%d.%d", verMaj, verMin, verPat);
    }
    wchar_t stage[MAX_PATH];
    swprintf(stage, MAX_PATH, L"%ls\\stage", root);
    CreateDirectoryW(stage, NULL);
    wchar_t jsonpath[MAX_PATH];
    swprintf(jsonpath, MAX_PATH, L"%ls\\version.json", stage);
    char jsonpathA[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, jsonpath, -1, jsonpathA, MAX_PATH, NULL, NULL);

    FILE *jf = fopen(jsonpathA, "wb");
    if (!jf) {
        printf("无法创建 version.json\n");
        return 3;
    }
    char noteEsc[2048] = {0};
    {
        wchar_t clog[MAX_PATH];
        swprintf(clog, MAX_PATH, L"%ls\\changelog.txt", root);
        char noteBuf[1024] = {0};
        FILE *cf = _wfopen(clog, L"rb");
        if (cf) {
            size_t nr = fread(noteBuf, 1, 1023, cf);
            fclose(cf);
            while (nr > 0 && (noteBuf[nr - 1] == '\n' || noteBuf[nr - 1] == '\r')) { nr--; noteBuf[nr] = 0; }
            int ei = 0;
            for (int i = 0; noteBuf[i] && i < 1023 && ei < 2046; i++) {
                if (noteBuf[i] == '"' || noteBuf[i] == '\\') { noteEsc[ei++] = '\\'; noteEsc[ei++] = noteBuf[i]; }
                else noteEsc[ei++] = noteBuf[i];
            }
            noteEsc[ei] = 0;
        }
    }
    fprintf(jf, "{\n  \"version\": %d,\n  \"version_str\": \"%s\",\n  \"note\": \"%s\",\n  \"files\": [\n", verMaj * 10000 + verMin * 100 + verPat, verstr, noteEsc);
    char md5hexstr[33];
    int first = 1;
    for (int i = 0; i < nfiles; i++) {
        if (!files[i].injson) continue;
        unsigned char *data;
        size_t len;
        if (!readfile(files[i].local, &data, &len)) {
            fprintf(jf, "    {\"path\":\"%s\",\"size\":0,\"md5\":\"missing\"}", files[i].key);
            first = 0;
            continue;
        }
        md5hex(data, len, md5hexstr);
        if (!first) fprintf(jf, ",\n");
        fprintf(jf, "    {\"path\":\"%s\",\"size\":%lu,\"md5\":\"%s\"}", files[i].key, (unsigned long)len, md5hexstr);
        first = 0;
        free(data);
    }
    fprintf(jf, "\n  ],\n  \"remove\": [");
    for (int i = 0; i < ndels; i++) {
        char delA[1024];
        WideCharToMultiByte(CP_UTF8, 0, dels[i], -1, delA, 1024, NULL, NULL);
        if (i) fprintf(jf, ",");
        fprintf(jf, "\n    \"%s\"", delA);
    }
    if (ndels) fprintf(jf, "\n  ");
    fprintf(jf, "]\n}\n");
    fclose(jf);

    vf = _wfopen(verfile, L"w");
    if (vf) {
        fprintf(vf, "%s", verstr);
        fclose(vf);
    }
    wchar_t vtpath[MAX_PATH];
    swprintf(vtpath, MAX_PATH, L"%ls\\version.txt", stage);
    vf = _wfopen(vtpath, L"w");
    if (vf) {
        fprintf(vf, "%s", verstr);
        fclose(vf);
    }

    wchar_t err[512];
    int failcount = 0;
    int succount = 0;
    wchar_t msg[4096];
    swprintf(msg, 4096, L"开始上传版本 v%hs (%ls渠道) 到 R2...\n\n", verstr, isQuanneng ? L"全能版" : L"高端版");
    for (int i = 0; i < nfiles; i++) {
        if (files[i].skipupload) {
            wchar_t m2[300];
            swprintf(m2, 300, L"跳过(未变化) %hs\n", files[i].key);
            wcscat(msg, m2);
            continue;
        }
        if (GetFileAttributesW(files[i].local) == INVALID_FILE_ATTRIBUTES) {
            wchar_t m2[600];
            swprintf(m2, 600, L"跳过(不存在): %ls\n", files[i].local);
            wcscat(msg, m2);
            continue;
        }
        wchar_t args[9000];
        char enckey[1024];
        urlenc(files[i].key, enckey, 1024);
        swprintf(args, 9000, L"--aws-sigv4 \"aws:amz:auto:s3\" --user \"%hs:%hs\" -X PUT -H \"Cache-Control: no-store\" --data-binary \"@%ls\" \"https://%hs/%hs/%ls%hs\"",
            accesskey, secret, files[i].local, endpoint, R2_BUCKET, channelPath, enckey);
        if (run_curl(args, err, 512) != 0) {
            wchar_t m2[600];
            swprintf(m2, 600, L"失败 %hs: %ls\n", files[i].key, err);
            wcscat(msg, m2);
            failcount++;
        } else {
            wchar_t m2[300];
            swprintf(m2, 300, L"成功 %hs\n", files[i].key);
            wcscat(msg, m2);
            succount++;
            files[i].uploaded = 1;
        }
    }
    wchar_t jsonkeyargs[9000];
    swprintf(jsonkeyargs, 9000, L"--aws-sigv4 \"aws:amz:auto:s3\" --user \"%hs:%hs\" -X PUT -H \"Cache-Control: no-store\" --data-binary \"@%ls\" \"https://%hs/%hs/%lsversion.json\"",
        accesskey, secret, jsonpath, endpoint, R2_BUCKET, channelPath);
    if (run_curl(jsonkeyargs, err, 512) != 0) {
        wchar_t m2[600];
        swprintf(m2, 600, L"失败 version.json: %ls\n", err);
        wcscat(msg, m2);
        failcount++;
    } else {
        wcscat(msg, L"成功 version.json\n");
        succount++;
    }
    wchar_t vtargs[9000];
    swprintf(vtargs, 9000, L"--aws-sigv4 \"aws:amz:auto:s3\" --user \"%hs:%hs\" -X PUT -H \"Cache-Control: no-store\" --data-binary \"@%ls\" \"https://%hs/%hs/%lsversion.txt\"",
        accesskey, secret, vtpath, endpoint, R2_BUCKET, channelPath);
    if (run_curl(vtargs, err, 512) != 0) {
        wchar_t m2[600];
        swprintf(m2, 600, L"失败 version.txt: %ls\n", err);
        wcscat(msg, m2);
        failcount++;
    } else {
        wcscat(msg, L"成功 version.txt\n");
        succount++;
    }
    FILE *mf = _wfopen(manifestPath, L"w");
    if (mf) {
        for (int i = 0; i < nfiles; i++)
            if (files[i].uploaded)
                fprintf(mf, "%s|%s\n", files[i].key, files[i].md5);
        fclose(mf);
    }
    wchar_t result[4200];
    swprintf(result, 4200, L"%ls\n成功 %d 项, 失败 %d 项", msg, succount, failcount);
    if (silent) {
        wchar_t rp[MAX_PATH];
        swprintf(rp, MAX_PATH, L"%ls\\push_result.txt", root);
        FILE *rf = _wfopen(rp, L"w, ccs=UTF-8");
        if (rf) {
            fwprintf(rf, L"%ls", result);
            fclose(rf);
        }
    }
    char u8[14000];
    WideCharToMultiByte(CP_UTF8, 0, result, -1, u8, sizeof(u8), NULL, NULL);
    printf("%s\n", u8);
    return failcount ? 1 : 0;
}
