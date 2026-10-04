# WallpaperLoader

x64 Windows 原生 DLL, 通过 JNI 把**当前桌面壁纸**以 RGBA 像素流送进 Java,
方便用 LWJGL / OpenGL 上传为纹理并渲染。

包名: `dev.yuxing.irotoridoriNoSekai.WallpaperLoader`

## 功能

| 来源 | 方式 | 画面 | 备注 |
|---|---|---|---|
| Steam Wallpaper Engine | WorkerW 壁纸窗口独立捕获 (Windows.Graphics.Capture) | 实时、**纯壁纸**不含窗口 | 需 Win10 1803+ |
| Lively / 其他动态壁纸引擎 | DXGI 桌面复制 | 实时、画面为合成后桌面 (**含窗口/图标**) | 通用兜底 |
| Windows 原生壁纸 | 读取系统壁纸文件 + WIC 解码 | 静态单帧 | 无动态壁纸时自动选用 |

`SOURCE_AUTO` (默认) 自动选择: 检测到 `wallpaper64.exe`/`wallpaper32.exe`
→ 尝试独立窗口捕获, 失败则回退桌面复制; 检测到 `Lively.exe` → 桌面复制;
否则 → Windows 原生静态壁纸。也可用 `SOURCE_*` 常量强制指定。

像素格式: RGBA, bottom-up, 行跨度 `width*4`, 可直接
`glTexImage2D` / `glTexSubImage2D` (`GL_RGBA` / `GL_UNSIGNED_BYTE`) 上传。

## 目录结构

```
wallpaper-loader/
├── CMakeLists.txt
├── build-msvc.bat              # Windows 一键构建脚本
├── README.md
├── native/
│   ├── wallpaper_loader.h      # 核心接口 (FrameBuffer / IWallpaperSource / Core)
│   ├── jni_bridge.cpp          # JNI 胶水 (包名 dev.yuxing.irotoridoriNoSekai)
│   ├── core.cpp                # 来源自动选择、帧更新、FPS 统计
│   ├── util.h / util.cpp       # 进程检测、BGRA→RGBA 翻转、错误信息
│   ├── source_static.cpp       # 后端1: Windows 原生壁纸 (SPI + WIC)
│   ├── source_dxgi.cpp         # 后端2: DXGI 桌面复制 (通用实时兜底)
│   └── source_we_graphics.cpp  # 后端3: Wallpaper Engine 窗口独立捕获 (MSVC only)
└── java/
    └── dev/yuxing/irotoridoriNoSekai/
        ├── WallpaperLoader.java  # Java API (AutoCloseable)
        └── WallpaperDemo.java    # LWJGL 3 实时渲染演示
```

## 构建 (Windows x64)

前置: Visual Studio 2022 Build Tools (C++ 桌面开发 + Windows SDK)、CMake、JDK 17+,
设置 `JAVA_HOME` 环境变量。

```bat
build-msvc.bat
```

产物: `build\Release\WallpaperLoader.dll`

> 说明: `source_we_graphics.cpp` 用了 C++/WinRT, 只能在 MSVC + Windows SDK 下编译。
> 非 MSVC 工具链构建时会自动跳过该文件 (Wallpaper Engine 走 DXGI 兜底)。

## Java 使用

```java
try (WallpaperLoader loader = new WallpaperLoader()) { // 或指定来源
    System.out.println(loader.getSourceName());
    loader.update();
    int w = loader.getWidth(), h = loader.getHeight();
    ByteBuffer px = loader.getPixels(); // RGBA direct buffer
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);

    while (running) {
        int r = loader.update();
        if (r == WallpaperLoader.UPDATE_NEW_FRAME) {
            // glTexSubImage2D(...) 上传 px
        } else if (r == WallpaperLoader.UPDATE_RESIZED) {
            px = loader.getPixels(); // 重新获取
            // glTexImage2D(...) 重新分配纹理
        }
    }
}
```

运行参数示例:

```
java -Djava.library.path=D:\wallpaper-loader\build\Release -cp ... dev.yuxing.irotoridoriNoSekai.WallpaperDemo
```

LWJGL 依赖见 `WallpaperDemo.java` 头部注释 (Gradle, LWJGL 3.3.3 BOM)。

## API 一览 (native 方法均已封装)

- `new WallpaperLoader(int preferredSource)` / `close()`
- `update()` → `UPDATE_NO_FRAME(0)` / `UPDATE_NEW_FRAME(1)` / `UPDATE_RESIZED(2)`
- `getPixels()` → RGBA `ByteBuffer` (direct)
- `getWidth()` / `getHeight()` / `getFps()`
- `getSourceType()` → `SOURCE_*` (实际使用的来源)
- `getSourceName()` → 来源描述字符串

## 注意事项

1. **DXGI 兜底画面含窗口**: Lively 等引擎走桌面复制时, 像素是合成后的整个桌面。
   只有 Wallpaper Engine 的独立窗口捕获能拿到纯壁纸。
2. **Wallpaper Engine 窗口定位**依赖其经典实现 (壁纸窗口挂在 WorkerW 下且归属
   `wallpaper64.exe` 进程); 若未来版本改动, 会自动回退到桌面复制。
3. **多显示器**: DXGI 后端采集主显示器; WE 独立捕获跟随壁纸窗口本身。
4. **线程**: `update()` 非阻塞轮询, 在调用线程执行; 建议在渲染线程每帧调用一次。
5. **验证状态**: 本代码在 Linux 环境编写, 未经 Windows + MSVC 实机编译验证。
   构建后请先跑 `WallpaperDemo` 确认三路来源切换正常。
