#pragma once
// WallpaperLoader 核心接口
// x64 Windows DLL, C++17, MSVC
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <deque>
#include <chrono>

// 与 Java 侧 WallpaperLoader.SOURCE_* 保持一致
enum class WallpaperSourceType : int {
    Unknown            = 0,
    WindowsNative      = 1,  // Windows 原生壁纸 (静态文件, WIC 解码)
    WallpaperEngine    = 2,  // Wallpaper Engine (WorkerW 窗口独立捕获, 实时, 不含窗口)
    Lively             = 3,  // Lively Wallpaper (已检测到, 走桌面复制)
    DesktopDuplication = 4,  // 通用 DXGI 桌面复制 (实时, 画面含窗口/图标)
};

enum class PreferredSource : int {
    Auto               = 0,
    WindowsNative      = 1,
    WallpaperEngine    = 2,
    Lively             = 3,
    DesktopDuplication = 4,
};

// RGBA 像素, 自下而上排列 (OpenGL 纹理可直接上传), 行跨度 = width * 4
struct FrameBuffer {
    std::vector<uint8_t> pixels;
    int width  = 0;
    int height = 0;

    void resize(int w, int h) {
        if (w == width && h == height && !pixels.empty()) return;
        width = w; height = h;
        pixels.assign((size_t)w * h * 4, 0);
    }
    bool empty() const { return pixels.empty(); }
    size_t byteSize() const { return pixels.size(); }
};

class IWallpaperSource {
public:
    virtual ~IWallpaperSource() = default;
    virtual bool open(std::string& err) = 0;
    virtual void close() = 0;
    // 返回 true 表示拿到新帧并已写入 frame; false 表示无新帧 (err 仅在出错时设置)
    virtual bool update(FrameBuffer& frame, std::string& err) = 0;
    virtual WallpaperSourceType type() const = 0;
    virtual std::string describe() const = 0;
};

// 后端工厂 (各实现在自己的 .cpp 中)
std::unique_ptr<IWallpaperSource> CreateStaticWallpaperSource();
std::unique_ptr<IWallpaperSource> CreateDxgiDuplicationSource();
#ifdef WALLPAPER_HAS_WE_GRAPHICS
std::unique_ptr<IWallpaperSource> CreateWallpaperEngineGraphicsSource();
#endif

class WallpaperLoaderCore {
public:
    WallpaperLoaderCore();
    ~WallpaperLoaderCore();

    bool init(int preferredSource, std::string& err);
    void dispose();

    // 返回值: 0=无新帧, 1=新帧(尺寸不变), 2=新帧(尺寸变化, Java 需重新获取 ByteBuffer)
    int update(std::string& err);

    FrameBuffer& frame() { return frame_; }
    int width() const { return frame_.width; }
    int height() const { return frame_.height; }
    WallpaperSourceType sourceType() const;
    std::string sourceName() const;
    int fps() const;

private:
    void noteFrame();

    std::unique_ptr<IWallpaperSource> source_;
    FrameBuffer frame_;
    std::deque<std::chrono::steady_clock::time_point> frameTimes_;
};
