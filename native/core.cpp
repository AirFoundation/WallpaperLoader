// 核心调度: 后端自动选择 / 帧更新 / FPS 统计
#include "wallpaper_loader.h"
#include "util.h"

WallpaperLoaderCore::WallpaperLoaderCore() = default;
WallpaperLoaderCore::~WallpaperLoaderCore() { dispose(); }

bool WallpaperLoaderCore::init(int preferredSource, std::string& err) {
    dispose();
    const auto pref = static_cast<PreferredSource>(preferredSource);

    const bool weRunning =
        wl::IsProcessRunning(L"wallpaper64.exe") || wl::IsProcessRunning(L"wallpaper32.exe");
    const bool livelyRunning = wl::IsProcessRunning(L"Lively.exe");

    auto tryOpen = [&](std::unique_ptr<IWallpaperSource> s) -> bool {
        std::string e;
        if (s->open(e)) { source_ = std::move(s); return true; }
        err = e;
        return false;
    };

    // 显式指定来源
    switch (pref) {
    case PreferredSource::WindowsNative:
        return tryOpen(CreateStaticWallpaperSource());
    case PreferredSource::WallpaperEngine:
#ifdef WALLPAPER_HAS_WE_GRAPHICS
        if (tryOpen(CreateWallpaperEngineGraphicsSource())) return true;
        err.clear(); // 独立捕获失败则回退到桌面复制
#endif
        return tryOpen(CreateDxgiDuplicationSource());
    case PreferredSource::Lively:
    case PreferredSource::DesktopDuplication:
        return tryOpen(CreateDxgiDuplicationSource());
    case PreferredSource::Auto:
    default:
        break;
    }

    // 自动选择
#ifdef WALLPAPER_HAS_WE_GRAPHICS
    if (weRunning) {
        auto we = CreateWallpaperEngineGraphicsSource();
        std::string e;
        if (we->open(e)) { source_ = std::move(we); return true; }
        // 找不到壁纸窗口则回退到桌面复制, 仍报告为 WallpaperEngine 来源
    }
#endif
    if (weRunning || livelyRunning) {
        return tryOpen(CreateDxgiDuplicationSource());
    }
    return tryOpen(CreateStaticWallpaperSource());
}

void WallpaperLoaderCore::dispose() {
    if (source_) { source_->close(); source_.reset(); }
    frame_ = FrameBuffer{};
    frameTimes_.clear();
}

int WallpaperLoaderCore::update(std::string& err) {
    if (!source_) { err = "WallpaperLoader 尚未初始化"; return 0; }
    const int ow = frame_.width, oh = frame_.height;
    if (!source_->update(frame_, err)) return 0;
    noteFrame();
    if (frame_.width != ow || frame_.height != oh) return 2;
    return 1;
}

WallpaperSourceType WallpaperLoaderCore::sourceType() const {
    return source_ ? source_->type() : WallpaperSourceType::Unknown;
}

std::string WallpaperLoaderCore::sourceName() const {
    return source_ ? source_->describe() : std::string("uninitialized");
}

void WallpaperLoaderCore::noteFrame() {
    using clock = std::chrono::steady_clock;
    const auto now = clock::now();
    frameTimes_.push_back(now);
    const auto cutoff = now - std::chrono::seconds(1);
    while (!frameTimes_.empty() && frameTimes_.front() < cutoff) frameTimes_.pop_front();
}

int WallpaperLoaderCore::fps() const {
    return static_cast<int>(frameTimes_.size());
}
