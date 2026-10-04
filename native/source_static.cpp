// 后端1: Windows 原生壁纸 (静态)
// 通过 SPI_GETDESKWALLPAPER 取当前壁纸文件路径, WIC 解码为 RGBA。
#include "wallpaper_loader.h"
#include "util.h"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <cstring>

#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;

class StaticWallpaperSource : public IWallpaperSource {
public:
    bool open(std::string& err) override {
        wchar_t path[MAX_PATH] = {};
        if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, path, 0) || path[0] == L'\0') {
            err = "无法获取当前壁纸路径 (SPI_GETDESKWALLPAPER 失败或壁纸为空)";
            return false;
        }
        path_ = path;

        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        comInit_ = SUCCEEDED(hr); // S_FALSE 表示本线程已初始化过, 同样需要配对 Uninitialize
        if (FAILED(hr)) { err = "COM 初始化失败"; return false; }

        if (!decode(err)) { close(); return false; }
        return true;
    }

    void close() override {
        if (comInit_) { CoUninitialize(); comInit_ = false; }
        pixels_.clear();
        delivered_ = false;
    }

    bool update(FrameBuffer& frame, std::string&) override {
        if (delivered_) return false; // 静态壁纸只给一帧
        frame.resize(w_, h_);
        std::memcpy(frame.pixels.data(), pixels_.data(), pixels_.size());
        delivered_ = true;
        return true;
    }

    WallpaperSourceType type() const override { return WallpaperSourceType::WindowsNative; }
    std::string describe() const override {
        return "Windows 原生壁纸: " + wl::WidenToUtf8(path_.c_str());
    }

private:
    bool decode(std::string& err) {
        ComPtr<IWICImagingFactory> factory;
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                     CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
        if (FAILED(hr)) { err = "WIC 工厂创建失败"; return false; }

        ComPtr<IWICBitmapDecoder> decoder;
        hr = factory->CreateDecoderFromFilename(path_.c_str(), nullptr, GENERIC_READ,
                                               WICDecodeMetadataCacheOnDemand, &decoder);
        if (FAILED(hr)) { err = "壁纸文件解码器创建失败: " + wl::WidenToUtf8(path_.c_str()); return false; }

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) { err = "无法读取壁纸第一帧"; return false; }

        ComPtr<IWICFormatConverter> conv;
        hr = factory->CreateFormatConverter(&conv);
        if (FAILED(hr)) { err = "WIC 格式转换器创建失败"; return false; }
        hr = conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                              WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) { err = "像素格式转换失败"; return false; }

        UINT w = 0, h = 0;
        conv->GetSize(&w, &h);
        if (w == 0 || h == 0) { err = "壁纸尺寸无效"; return false; }

        std::vector<uint8_t> topDown((size_t)w * h * 4);
        hr = conv->CopyPixels(nullptr, w * 4, (UINT)topDown.size(), topDown.data());
        if (FAILED(hr)) { err = "像素拷贝失败"; return false; }

        // 翻转为 OpenGL bottom-up
        pixels_.resize(topDown.size());
        for (UINT y = 0; y < h; ++y) {
            std::memcpy(pixels_.data() + (size_t)y * w * 4,
                        topDown.data() + (size_t)(h - 1 - y) * w * 4, (size_t)w * 4);
        }
        w_ = (int)w; h_ = (int)h;
        return true;
    }

    std::wstring path_;
    std::vector<uint8_t> pixels_; // 已翻转为 bottom-up RGBA
    int w_ = 0, h_ = 0;
    bool comInit_ = false;
    bool delivered_ = false;
};

std::unique_ptr<IWallpaperSource> CreateStaticWallpaperSource() {
    return std::make_unique<StaticWallpaperSource>();
}
