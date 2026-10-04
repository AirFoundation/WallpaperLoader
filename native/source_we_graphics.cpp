// 后端3: Wallpaper Engine 独立窗口捕获 (实时, 纯壁纸不含窗口)
// 原理: Wallpaper Engine 把渲染窗口挂在桌面图标层之下的 WorkerW 窗口上。
// 本后端定位该窗口 (归属 wallpaper64.exe / wallpaper32.exe 进程), 再用
// Windows.Graphics.Capture (WinRT, 需 Win10 1803+) 对该窗口单独采集。
// 仅 MSVC + Windows SDK 可编译 (C++/WinRT 头文件)。
#include "wallpaper_loader.h"
#include "util.h"

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

// ---- C++/WinRT ----
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "windowsapp.lib")

using Microsoft::WRL::ComPtr;
namespace wgc = winrt::Windows::Graphics::Capture;
namespace wgdx = winrt::Windows::Graphics::DirectX::Direct3D11;

namespace {

// 在顶层 WorkerW 窗口的子窗口中, 找到归属 Wallpaper Engine 进程的那个
BOOL CALLBACK EnumWorkerWProc(HWND hwnd, LPARAM lParam) {
    wchar_t cls[64] = {};
    GetClassNameW(hwnd, cls, 64);
    if (wcscmp(cls, L"WorkerW") != 0) return TRUE;
    for (HWND child = FindWindowExW(hwnd, nullptr, nullptr, nullptr);
         child != nullptr;
         child = FindWindowExW(hwnd, child, nullptr, nullptr)) {
        DWORD pid = 0;
        GetWindowThreadProcessId(child, &pid);
        if (wl::IsPidNamed(pid, L"wallpaper64.exe") || wl::IsPidNamed(pid, L"wallpaper32.exe")) {
            *reinterpret_cast<HWND*>(lParam) = child;
            return FALSE; // 停止枚举
        }
    }
    return TRUE;
}

HWND FindWallpaperEngineWindow() {
    // 触发系统生成承载壁纸的 WorkerW 窗口 (经典 0x052C 技巧)
    HWND progman = FindWindowW(L"Progman", L"Program Manager");
    if (progman) {
        DWORD_PTR res = 0;
        SendMessageTimeoutW(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &res);
    }
    HWND found = nullptr;
    EnumWindows(EnumWorkerWProc, reinterpret_cast<LPARAM>(&found));
    return found;
}

} // namespace

struct WEImpl {
    HWND hwnd = nullptr;
    winrt::com_ptr<ID3D11Device> d3d;
    winrt::com_ptr<ID3D11DeviceContext> ctx;
    wgc::GraphicsCaptureItem item{ nullptr };
    wgdx::Direct3D11CaptureFramePool pool{ nullptr };
    wgc::GraphicsCaptureSession session{ nullptr };
    winrt::com_ptr<ID3D11Texture2D> staging;
    int sw = 0, sh = 0; // staging 尺寸
    int w = 0, h = 0;   // 当前捕获尺寸
};

class WallpaperEngineGraphicsSource : public IWallpaperSource {
public:
    WallpaperEngineGraphicsSource() : impl_(std::make_unique<WEImpl>()) {}
    ~WallpaperEngineGraphicsSource() override { close(); }

    bool open(std::string& err) override {
        try {
            if (!winrt::Windows::Foundation::Metadata::ApiInformation::IsTypePresent(
                    L"Windows.Graphics.Capture.GraphicsCaptureSession")) {
                err = "当前系统不支持 Windows.Graphics.Capture (需要 Windows 10 1803 或更高)";
                return false;
            }

            HWND hwnd = FindWallpaperEngineWindow();
            if (!hwnd) {
                err = "未找到 Wallpaper Engine 的壁纸窗口 (请确认 wallpaper64.exe 正在运行且已应用壁纸)";
                return false;
            }
            impl_->hwnd = hwnd;

            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            apartmentInit_ = true;

            static const D3D_FEATURE_LEVEL levels[] = {
                D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
            };
            winrt::check_hresult(D3D11CreateDevice(
                nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT, // FramePool 必需
                levels, _countof(levels), D3D11_SDK_VERSION,
                impl_->d3d.put(), nullptr, impl_->ctx.put()));

            // 为 HWND 创建 GraphicsCaptureItem
            auto factory = winrt::get_activation_factory<wgc::GraphicsCaptureItem,
                                                         IGraphicsCaptureItemInterop>();
            winrt::check_hresult(factory->CreateForWindow(
                hwnd, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
                winrt::put_abi(impl_->item)));

            // D3D11Device -> WinRT IDirect3DDevice
            winrt::com_ptr<IDXGIDevice> dxgiDev;
            winrt::check_hresult(impl_->d3d->QueryInterface(IID_PPV_ARGS(dxgiDev.put())));
            wgdx::IDirect3DDevice directDev{ nullptr };
            winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(
                dxgiDev.get(), reinterpret_cast<IInspectable**>(winrt::put_abi(directDev))));

            if (!startSession(directDev, err)) { close(); return false; }
            return true;
        } catch (const winrt::hresult_error& e) {
            err = std::string("Wallpaper Engine 捕获初始化失败: ") +
                  wl::WidenToUtf8(e.message().c_str());
            close();
            return false;
        } catch (const std::exception& e) {
            err = std::string("Wallpaper Engine 捕获初始化失败: ") + e.what();
            close();
            return false;
        }
    }

    void close() override {
        try {
            if (impl_->session) { impl_->session.Close(); impl_->session = nullptr; }
            if (impl_->pool) { impl_->pool.Close(); impl_->pool = nullptr; }
            impl_->item = nullptr;
        } catch (...) {}
        impl_->staging.reset();
        impl_->ctx.reset();
        impl_->d3d.reset();
        impl_->hwnd = nullptr;
        impl_->sw = impl_->sh = impl_->w = impl_->h = 0;
        if (apartmentInit_) { winrt::uninit_apartment(); apartmentInit_ = false; }
    }

    bool update(FrameBuffer& frame, std::string& err) override {
        try {
            // 壁纸分辨率变化时重建会话
            auto sz = impl_->item.Size();
            if (sz.Width != impl_->w || sz.Height != impl_->h) {
                wgdx::IDirect3DDevice dd = currentDirectDevice(err);
                if (!dd) return false;
                if (!startSession(dd, err)) return false;
            }

            auto capFrame = impl_->pool.TryGetNextFrame();
            if (!capFrame) return false; // 无新帧

            auto surface = capFrame.Surface();
            auto access = surface.as<IDirect3DDxgiInterfaceAccess>();
            winrt::com_ptr<ID3D11Texture2D> tex;
            winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(tex.put())));

            D3D11_TEXTURE2D_DESC td{};
            tex->GetDesc(&td);
            if (!impl_->staging || (int)td.Width != impl_->sw || (int)td.Height != impl_->sh) {
                D3D11_TEXTURE2D_DESC sd = td;
                sd.BindFlags = 0;
                sd.MiscFlags = 0;
                sd.MipLevels = 1;
                sd.ArraySize = 1;
                sd.SampleDesc.Count = 1;
                sd.SampleDesc.Quality = 0;
                sd.Usage = D3D11_USAGE_STAGING;
                sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                winrt::check_hresult(impl_->d3d->CreateTexture2D(&sd, nullptr, impl_->staging.put()));
                impl_->sw = (int)td.Width; impl_->sh = (int)td.Height;
            }

            impl_->ctx->CopyResource(impl_->staging.get(), tex.get());
            D3D11_MAPPED_SUBRESOURCE mapped{};
            winrt::check_hresult(impl_->ctx->Map(impl_->staging.get(), 0, D3D11_MAP_READ, 0, &mapped));

            frame.resize(impl_->sw, impl_->sh);
            wl::BgraToRgbaFlip(static_cast<const uint8_t*>(mapped.pData), mapped.RowPitch,
                               frame.pixels.data(), impl_->sw, impl_->sh);
            impl_->ctx->Unmap(impl_->staging.get(), 0);
            capFrame.Close();
            return true;
        } catch (const winrt::hresult_error& e) {
            err = std::string("壁纸帧采集失败: ") + wl::WidenToUtf8(e.message().c_str());
            return false;
        } catch (const std::exception& e) {
            err = std::string("壁纸帧采集失败: ") + e.what();
            return false;
        }
    }

    WallpaperSourceType type() const override { return WallpaperSourceType::WallpaperEngine; }
    std::string describe() const override {
        return "Wallpaper Engine (WorkerW 窗口独立捕获, " +
               std::to_string(impl_->w) + "x" + std::to_string(impl_->h) + ", 纯壁纸)";
    }

private:
    bool startSession(const wgdx::IDirect3DDevice& dd, std::string& err) {
        try {
            if (impl_->session) { impl_->session.Close(); impl_->session = nullptr; }
            if (impl_->pool) { impl_->pool.Close(); impl_->pool = nullptr; }
            auto sz = impl_->item.Size();
            impl_->pool = wgdx::Direct3D11CaptureFramePool::Create(
                dd, wgdx::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, sz);
            impl_->session = impl_->pool.CreateCaptureSession(impl_->item);
            impl_->session.IsCursorCaptureEnabled(false); // 壁纸不需要光标
            impl_->session.StartCapture();
            impl_->w = sz.Width; impl_->h = sz.Height;
            return true;
        } catch (const winrt::hresult_error& e) {
            err = std::string("捕获会话启动失败: ") + wl::WidenToUtf8(e.message().c_str());
            return false;
        }
    }

    // 从当前 pool 反推 IDirect3DDevice (尺寸变化重建时用); 简化起见重建整个 open 流程的设备部分
    wgdx::IDirect3DDevice currentDirectDevice(std::string& err) {
        try {
            winrt::com_ptr<IDXGIDevice> dxgiDev;
            winrt::check_hresult(impl_->d3d->QueryInterface(IID_PPV_ARGS(dxgiDev.put())));
            wgdx::IDirect3DDevice dd{ nullptr };
            winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(
                dxgiDev.get(), reinterpret_cast<IInspectable**>(winrt::put_abi(dd))));
            return dd;
        } catch (const winrt::hresult_error& e) {
            err = std::string("Direct3D 设备包装失败: ") + wl::WidenToUtf8(e.message().c_str());
            return nullptr;
        }
    }

    std::unique_ptr<WEImpl> impl_;
    bool apartmentInit_ = false;
};

std::unique_ptr<IWallpaperSource> CreateWallpaperEngineGraphicsSource() {
    return std::make_unique<WallpaperEngineGraphicsSource>();
}
