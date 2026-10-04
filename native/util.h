#pragma once
// 通用工具: 进程检测 / 像素转换 / 线程局部错误信息
#include <string>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

namespace wl {

// 进程名是否正在运行 (大小写不敏感, 如 L"wallpaper64.exe")
bool IsProcessRunning(const wchar_t* exeName);

// pid 对应的进程映像名是否为 exeName
bool IsPidNamed(DWORD pid, const wchar_t* exeName);

// BGRA(top-down, 任意行跨度) -> RGBA(bottom-up, 紧凑排列, OpenGL 可直接上传)
void BgraToRgbaFlip(const uint8_t* src, int srcPitch, uint8_t* dst, int w, int h);

std::string WidenToUtf8(const wchar_t* ws);

// 供 JNI static nativeLastError() 使用的线程局部错误串
std::string& LastError();

} // namespace wl
