#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#pragma comment(lib, "shell32.lib")

static void logmsg(const wchar_t *fmt, ...) {
    wchar_t buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(buf, 1023, fmt, ap);
    va_end(ap);
    wchar_t logp[MAX_PATH];
    GetModuleFileNameW(NULL, logp, MAX_PATH);
    wchar_t *slash = wcsrchr(logp, L'\\');
    if (slash) *slash = 0;
    wcscat(logp, L"\\updater.log");
    FILE *f = _wfopen(logp, L"ab");
    if (f) {
        char nb[2048];
        int n = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nb, 2047, NULL, NULL);
        if (n > 0) fwrite(nb, 1, n - 1, f);
        fputc('\n', f);
        fclose(f);
    }
}

static void wait_exit(DWORD pid, DWORD timeout_ms) {
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (h) {
        WaitForSingleObject(h, timeout_ms);
        CloseHandle(h);
    }
}

static int do_replace_retry(const wchar_t *src, const wchar_t *dst) {
    int attempt;
    for (attempt = 0; attempt < 6; attempt++) {
        if (GetFileAttributesW(src) == INVALID_FILE_ATTRIBUTES) return 0;
        if (MoveFileExW(src, dst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            if (GetFileAttributesW(dst) != INVALID_FILE_ATTRIBUTES) return 1;
        }
        Sleep(1000);
    }
    logmsg(L"MoveFileEx failed after retries: %s -> %s", src, dst);
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc < 3) return 1;
    DWORD parentPid = (DWORD)_wtoi(argv[2]);
    wait_exit(parentPid, 90000);
    wchar_t selfpath[MAX_PATH];
    GetModuleFileNameW(NULL, selfpath, MAX_PATH);
    wchar_t selfnew[MAX_PATH];
    wcscpy(selfnew, selfpath);
    wchar_t *selfslash = wcsrchr(selfnew, L'\\');
    if (selfslash) wcscpy(selfslash + 1, L"updater_new.exe");
    int selfStaged = 0;
    FILE *f = _wfopen(argv[1], L"rb");
    if (!f) return 2;
    wchar_t line[2048];
    wchar_t launch[MAX_PATH] = {0};
    int okCount = 0, failCount = 0;
    char raw[4096];
    while (fgets(raw, 4095, f)) {
        MultiByteToWideChar(CP_UTF8, 0, raw, -1, line, 2047);
        wchar_t *nl = wcschr(line, L'\n');
        if (nl) *nl = 0;
        wchar_t *cr = wcschr(line, L'\r');
        if (cr) *cr = 0;
        wchar_t *sep = wcschr(line, L'|');
        if (!sep) continue;
        *sep = 0;
        wchar_t *a = line;
        wchar_t *b = sep + 1;
        if (wcscmp(a, L"LAUNCH") == 0) {
            wcsncpy(launch, b, MAX_PATH - 1);
            continue;
        }
        if (wcscmp(a, L"VERSION") == 0) {
            wchar_t vf[MAX_PATH];
            GetModuleFileNameW(NULL, vf, MAX_PATH);
            wchar_t *slash = wcsrchr(vf, L'\\');
            if (slash) *slash = 0;
            wcscat(vf, L"\\version.txt");
            char nb[64];
            int n = WideCharToMultiByte(CP_UTF8, 0, b, -1, nb, 63, NULL, NULL);
            FILE *vfh = _wfopen(vf, L"wb");
            if (vfh) {
                if (n > 0) fwrite(nb, 1, n - 1, vfh);
                fclose(vfh);
            }
            logmsg(L"VERSION written: %ls", b);
            continue;
        }
        if (wcscmp(a, L"DEL") == 0) {
            int attempt;
            for (attempt = 0; attempt < 6; attempt++) {
                if (GetFileAttributesW(b) == INVALID_FILE_ATTRIBUTES) break;
                if (DeleteFileW(b)) break;
                Sleep(1000);
            }
            logmsg(L"DEL: %ls", b);
            continue;
        }
        if (_wcsicmp(b, selfpath) == 0) {
            CopyFileW(a, selfnew, FALSE);
            selfStaged = 1;
            logmsg(L"SELFUPDATE staged: %ls", selfnew);
            continue;
        }
        if (do_replace_retry(a, b)) {
            logmsg(L"OK: %s -> %s", a, b);
            okCount++;
        } else {
            logmsg(L"FAIL: %s -> %s", a, b);
            failCount++;
        }
    }
    fclose(f);
    logmsg(L"done ok=%d fail=%d", okCount, failCount);
    wchar_t tmpdir[MAX_PATH + 1];
    memset(tmpdir, 0, sizeof(tmpdir));
    wcsncpy(tmpdir, argv[1], MAX_PATH - 1);
    wchar_t *last = wcsrchr(tmpdir, L'\\');
    if (last) {
        *last = 0;
        tmpdir[wcslen(tmpdir) + 1] = 0;
        SHFILEOPSTRUCTW so;
        memset(&so, 0, sizeof(so));
        so.wFunc = FO_DELETE;
        so.pFrom = tmpdir;
        so.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
        SHFileOperationW(&so);
    }
    if (selfStaged) {
        wchar_t cmdline[16384];
        if (launch[0])
            swprintf(cmdline, 16384, L"cmd /c ping -n 3 127.0.0.1 >nul & move /y \"%ls\" \"%ls\" & start \"\" \"%ls\"",
                selfnew, selfpath, launch);
        else
            swprintf(cmdline, 16384, L"cmd /c ping -n 3 127.0.0.1 >nul & move /y \"%ls\" \"%ls\"",
                selfnew, selfpath);
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        memset(&si, 0, sizeof(si));
        memset(&pi, 0, sizeof(pi));
        si.cb = sizeof(si);
        CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        if (pi.hProcess) CloseHandle(pi.hProcess);
        if (pi.hThread) CloseHandle(pi.hThread);
    } else if (launch[0]) {
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        memset(&si, 0, sizeof(si));
        memset(&pi, 0, sizeof(pi));
        si.cb = sizeof(si);
        if (CreateProcessW(launch, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            if (pi.hProcess) CloseHandle(pi.hProcess);
            if (pi.hThread) CloseHandle(pi.hThread);
        }
    }
    LocalFree(argv);
    return 0;
}
