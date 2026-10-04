// 后端2: DXGI 桌面复制 (通用实时兜底)
// 适用于 Lively / 其他壁纸引擎, 以及 Wallpaper Engine 独立捕获失败时的回退。
// 注意: 采集的是合成后的整个桌面 (会包含窗口和图标), 非纯壁纸。
#include "wallpaper_loader.h"
#include "util.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

class DxgiDuplicationSource : public IWallpaperSource {
public:
    bool open(std::string& err) override {
        HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory_));
        if (FAILED(hr)) { err = "DXGI 工厂创建失败"; return false; }

        // 选择主显示器 (桌面坐标包含原点的输出)
        ComPtr<IDXGIAdapter1> adapter;
        ComPtr<IDXGIOutput> output;
        bool found = false;
        for (UINT ai = 0; !found && SUCCEEDED(factory_->EnumAdapters1(ai, &adapter)); ++ai) {
            for (UINT oi = 0; SUCCEEDED(adapter->EnumOutputs(oi, &output)); ++oi) {
                DXGI_OUTPUT_DESC desc{};
                if (SUCCEEDED(output->GetDesc(&desc)) && desc.AttachedToDesktop &&
                    desc.DesktopCoordinates.left <= 0 && desc.DesktopCoordinates.top <= 0 &&
                    desc.DesktopCoordinates.right > 0 && desc.DesktopCoordinates.bottom > 0) {
                    found = true;
                    break;
                }
                output.Reset();
            }
            if (!found) adapter.Reset();
        }
        if (!found) { err = "未找到已连接的主显示器"; return false; }

        static const D3D_FEATURE_LEVEL levels[] = {
            D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
        };
        hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                               D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                               levels, _countof(levels), D3D11_SDK_VERSION,
                               &device_, nullptr, &ctx_);
        if (FAILED(hr)) { err = "D3D11 设备创建失败"; return false; }

        ComPtr<IDXGIOutput1> out1;
        hr = output->QueryInterface(IID_PPV_ARGS(&out1));
        if (FAILED(hr)) { err = "该显示输出不支持桌面复制 (需要 WDDM 1.2+)"; return false; }

        hr = out1->DuplicateOutput(device_.Get(), &dup_);
        if (FAILED(hr)) {
            err = "DuplicateOutput 失败 (可能被独占全屏应用占用, 或已达复制会话上限)";
            return false;
        }

        DXGI_OUTPUT_DESC desc{};
        output->GetDesc(&desc);
        w_ = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
        h_ = desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top;
        return true;
    }

    void close() override {
        staging_.Reset();
        dup_.Reset();
        ctx_.Reset();
        device_.Reset();
        factory_.Reset();
        sw_ = sh_ = 0;
    }

    bool update(FrameBuffer& frame, std::string& err) override {
        DXGI_OUTDUPL_FRAME_INFO info{};
        ComPtr<IDXGIResource> res;
        HRESULT hr = dup_->AcquireNextFrame(0, &info, &res); // 非阻塞轮询
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) return false;     // 无新帧
        if (FAILED(hr)) { err = "AcquireNextFrame 失败 (访问丢失时可重建采集器)"; return false; }

        struct FrameReleaser {
            IDXGIOutputDuplication* d;
            ~FrameReleaser() { if (d) d->ReleaseFrame(); }
        } releaser{ dup_.Get() };

        ComPtr<ID3D11Texture2D> tex;
        hr = res->QueryInterface(IID_PPV_ARGS(&tex));
        if (FAILED(hr)) { err = "帧纹理获取失败"; return false; }

        D3D11_TEXTURE2D_DESC td{};
        tex->GetDesc(&td);
        if (!staging_ || (int)td.Width != sw_ || (int)td.Height != sh_) {
            D3D11_TEXTURE2D_DESC sd = td;
            sd.BindFlags = 0;
            sd.MiscFlags = 0;
            sd.MipLevels = 1;
            sd.ArraySize = 1;
            sd.SampleDesc.Count = 1;
            sd.SampleDesc.Quality = 0;
            sd.Usage = D3D11_USAGE_STAGING;
            sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            hr = device_->CreateTexture2D(&sd, nullptr, &staging_);
            if (FAILED(hr)) { err = "CPU 可读 staging 纹理创建失败"; return false; }
            sw_ = (int)td.Width; sh_ = (int)td.Height;
        }

        ctx_->CopyResource(staging_.Get(), tex.Get());

        D3D11_MAPPED_SUBRESOURCE mapped{};
        hr = ctx_->Map(staging_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr)) { err = "纹理映射失败"; return false; }

        frame.resize(sw_, sh_);
        wl::BgraToRgbaFlip(static_cast<const uint8_t*>(mapped.pData), mapped.RowPitch,
                           frame.pixels.data(), sw_, sh_);
        ctx_->Unmap(staging_.Get(), 0);
        return true;
    }

    WallpaperSourceType type() const override { return WallpaperSourceType::DesktopDuplication; }
    std::string describe() const override {
        return "DXGI 桌面复制 (主显示器 " + std::to_string(w_) + "x" + std::to_string(h_) + ", 画面含窗口)";
    }

private:
    ComPtr<IDXGIFactory1> factory_;
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> ctx_;
    ComPtr<IDXGIOutputDuplication> dup_;
    ComPtr<ID3D11Texture2D> staging_;
    int w_ = 0, h_ = 0;   // 输出尺寸
    int sw_ = 0, sh_ = 0; // staging 尺寸
};

std::unique_ptr<IWallpaperSource> CreateDxgiDuplicationSource() {
    return std::make_unique<DxgiDuplicationSource>();
}
