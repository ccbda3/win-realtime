#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const wchar_t *APP_EXE = L"gps-ui-qt.exe";

static int findAppPath(wchar_t *out, int outChars) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    int found = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (lstrcmpiW(pe.szExeFile, APP_EXE) == 0) {
                HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                if (h) {
                    DWORD sz = (DWORD)outChars;
                    if (QueryFullProcessImageNameW(h, 0, out, &sz)) found = 1;
                    CloseHandle(h);
                }
                if (found) break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

static void killApp(void) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (lstrcmpiW(pe.szExeFile, APP_EXE) == 0) {
                HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID);
                if (h) {
                    TerminateProcess(h, 1);
                    WaitForSingleObject(h, 8000);
                    CloseHandle(h);
                }
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
}

static void launchApp(const wchar_t *path) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (CreateProcessW(path, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

static void getAppDir(const wchar_t *appPath, wchar_t *out) {
    wcscpy(out, appPath);
    wchar_t *slash = wcsrchr(out, L'\\');
    if (slash) *slash = 0;
}

static int writeRenderMode(const wchar_t *path, const char *newVal) {
    FILE *f = _wfopen(path, L"rb");
    if (!f) {
        f = _wfopen(path, L"wb");
        if (!f) return 0;
        fprintf(f, "%s\n", newVal);
        fclose(f);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc(sz + 1);
    if (!buf) { fclose(f); return 0; }
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = 0;
    fclose(f);
    const char *key = "renderMode=";
    char *pos = strstr(buf, key);
    int ok = 0;
    if (pos) {
        char *lineEnd = strchr(pos, '\n');
        if (!lineEnd) lineEnd = buf + rd;
        long prefixLen = pos - buf;
        long suffixLen = buf + rd - lineEnd;
        long vl = (long)strlen(newVal);
        long newLen = prefixLen + vl + 1 + suffixLen;
        char *nb = (char*)malloc(newLen + 1);
        if (nb) {
            memcpy(nb, buf, prefixLen);
            memcpy(nb + prefixLen, newVal, vl);
            nb[prefixLen + vl] = '\n';
            memcpy(nb + prefixLen + vl + 1, lineEnd, suffixLen);
            nb[newLen] = 0;
            f = _wfopen(path, L"wb");
            if (f) {
                fwrite(nb, 1, newLen, f);
                fclose(f);
                ok = 1;
            }
            free(nb);
        }
    } else {
        f = _wfopen(path, L"ab");
        if (f) {
            fprintf(f, "%s\n", newVal);
            fclose(f);
            ok = 1;
        }
    }
    free(buf);
    return ok;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(NULL, self, MAX_PATH);
    int setGpu = wcsstr(self, L"GPU") != NULL;
    const char *newVal = setGpu ? "renderMode=gpu" : "renderMode=cpu";

    wchar_t appPath[MAX_PATH];
    int hasApp = findAppPath(appPath, MAX_PATH);
    if (!hasApp) {
        wchar_t selfDir[MAX_PATH];
        getAppDir(self, selfDir);
        swprintf(appPath, MAX_PATH, L"%ls\\%ls", selfDir, APP_EXE);
        if (GetFileAttributesW(appPath) == INVALID_FILE_ATTRIBUTES) {
            MessageBoxW(NULL, L"未找到软件，请先启动软件后再切换渲染模式", L"渲染模式", MB_OK | MB_ICONWARNING);
            return 1;
        }
    }

    wchar_t dir[MAX_PATH];
    DWORD dlen = GetEnvironmentVariableW(L"APPDATA", dir, MAX_PATH);
    if (dlen == 0) return 1;
    wchar_t settingsPath[MAX_PATH];
    swprintf(settingsPath, MAX_PATH, L"%ls\\行迹自录\\PosNavTech\\settings.ini", dir);
    writeRenderMode(settingsPath, newVal);

    wchar_t appDir[MAX_PATH];
    getAppDir(appPath, appDir);
    wchar_t marker[MAX_PATH];
    swprintf(marker, MAX_PATH, L"%ls\\.crash_marker", appDir);
    DeleteFileW(marker);

    killApp();
    launchApp(appPath);

    MessageBoxW(NULL, setGpu ? L"已切换为GPU渲染，软件已自动重启" : L"已切换为CPU渲染，软件已自动重启", L"渲染模式", MB_OK | MB_ICONINFORMATION);
    return 0;
}
