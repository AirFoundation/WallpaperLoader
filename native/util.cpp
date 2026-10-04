// 通用工具实现
#include "util.h"
#include <tlhelp32.h>
#include <cwchar>
#include <cstring>

namespace wl {

bool IsProcessRunning(const wchar_t* exeName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    bool found = false;
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (_wcsicmp(pe.szExeFile, exeName) == 0) { found = true; break; }
    }
    CloseHandle(snap);
    return found;
}

bool IsPidNamed(DWORD pid, const wchar_t* exeName) {
    if (pid == 0) return false;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    // 用 QueryFullProcessImageNameW (kernel32) 取映像路径, 避免依赖 psapi
    wchar_t path[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    BOOL ok = QueryFullProcessImageNameW(h, 0, path, &size);
    CloseHandle(h);
    if (!ok || size == 0) return false;
    const wchar_t* base = wcsrchr(path, L'\\');
    base = base ? base + 1 : path;
    return _wcsicmp(base, exeName) == 0;
}

void BgraToRgbaFlip(const uint8_t* src, int srcPitch, uint8_t* dst, int w, int h) {
    const int dstPitch = w * 4;
    for (int y = 0; y < h; ++y) {
        const uint8_t* s = src + (size_t)y * srcPitch;
        uint8_t* d = dst + (size_t)(h - 1 - y) * dstPitch; // 上下翻转 -> OpenGL bottom-up
        for (int x = 0; x < w; ++x) {
            d[x * 4 + 0] = s[x * 4 + 2]; // R
            d[x * 4 + 1] = s[x * 4 + 1]; // G
            d[x * 4 + 2] = s[x * 4 + 0]; // B
            d[x * 4 + 3] = s[x * 4 + 3]; // A
        }
    }
}

std::string WidenToUtf8(const wchar_t* ws) {
    if (!ws || !*ws) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out((size_t)n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws, -1, out.data(), n, nullptr, nullptr);
    return out;
}

std::string& LastError() {
    thread_local std::string err;
    return err;
}

} // namespace wl
