package dev.yuxing.irotoridoriNoSekai;

import java.nio.ByteBuffer;

/**
 * 桌面壁纸采集器 (JNI, x64 Windows)。
 *
 * <p>把当前电脑的桌面壁纸以 RGBA 像素流的形式送进 Java, 方便用 LWJGL / OpenGL
 * 上传为纹理并渲染。支持三种来源, 默认自动选择:</p>
 * <ul>
 *   <li>{@link #SOURCE_WALLPAPER_ENGINE} - Steam Wallpaper Engine: 独立捕获壁纸窗口,
 *       实时、纯壁纸画面 (不含桌面窗口/图标), 需要 Win10 1803+</li>
 *   <li>{@link #SOURCE_DESKTOP_DUPLICATION} - 通用 DXGI 桌面复制: 适用于 Lively 等
 *       其他壁纸引擎的实时采集; 注意画面是合成后的整个桌面 (含窗口)</li>
 *   <li>{@link #SOURCE_WINDOWS_NATIVE} - Windows 原生壁纸: 读取系统壁纸文件, 静态单帧</li>
 * </ul>
 *
 * <p>典型用法 (LWJGL):</p>
 * <pre>{@code
 * try (WallpaperLoader loader = new WallpaperLoader()) {
 *     System.out.println("壁纸源: " + loader.getSourceName());
 *     loader.update();
 *     int tex = GL11.glGenTextures();
 *     // ... glTexImage2D 上传 loader.getPixels() ...
 *     while (running) {
 *         int r = loader.update();
 *         if (r == WallpaperLoader.UPDATE_NEW_FRAME) {
 *             // glTexSubImage2D 上传 loader.getPixels()
 *         } else if (r == WallpaperLoader.UPDATE_RESIZED) {
 *             // 尺寸变化: 重新 glTexImage2D 分配
 *         }
 *     }
 * }
 * }</pre>
 */
public class WallpaperLoader implements AutoCloseable {

    static {
        System.loadLibrary("WallpaperLoader"); // WallpaperLoader.dll
    }

    /** 自动选择 (默认): WE运行->独立捕获, 其他动态壁纸->桌面复制, 否则静态壁纸 */
    public static final int SOURCE_AUTO = 0;
    /** Windows 原生壁纸 (静态) */
    public static final int SOURCE_WINDOWS_NATIVE = 1;
    /** Wallpaper Engine (实时, 纯壁纸) */
    public static final int SOURCE_WALLPAPER_ENGINE = 2;
    /** Lively Wallpaper (实时, 经由桌面复制) */
    public static final int SOURCE_LIVELY = 3;
    /** 通用 DXGI 桌面复制 (实时, 画面含窗口) */
    public static final int SOURCE_DESKTOP_DUPLICATION = 4;

    /** update() 返回: 无新帧 */
    public static final int UPDATE_NO_FRAME = 0;
    /** update() 返回: 新帧, 尺寸不变, getPixels() 可直接上传 */
    public static final int UPDATE_NEW_FRAME = 1;
    /** update() 返回: 新帧且尺寸变化, getPixels() 已更新, 纹理需重新分配 */
    public static final int UPDATE_RESIZED = 2;

    private long handle;
    private ByteBuffer pixels;

    public WallpaperLoader() {
        this(SOURCE_AUTO);
    }

    /**
     * @param preferredSource 来源偏好, 取 SOURCE_* 常量
     * @throws IllegalStateException 初始化失败时抛出, 原因见异常信息
     */
    public WallpaperLoader(int preferredSource) {
        handle = nativeInit(preferredSource);
        if (handle == 0) {
            throw new IllegalStateException("WallpaperLoader 初始化失败: " + nativeLastError());
        }
    }

    /**
     * 拉取一帧壁纸。
     * @return UPDATE_NO_FRAME / UPDATE_NEW_FRAME / UPDATE_RESIZED
     */
    public int update() {
        int r = nativeUpdate(handle);
        if (r == UPDATE_NEW_FRAME || r == UPDATE_RESIZED || pixels == null) {
            pixels = nativeGetPixels(handle);
        }
        return r;
    }

    /** RGBA 直接内存 (bottom-up, 可直接 glTexImage2D/glTexSubImage2D 上传)。update() 后有效。 */
    public ByteBuffer getPixels() {
        return pixels;
    }

    public int getWidth()  { return nativeGetWidth(handle); }
    public int getHeight() { return nativeGetHeight(handle); }

    /** 实际使用的来源, 取 SOURCE_* 常量 (AUTO 会被解析为具体来源) */
    public int getSourceType() { return nativeGetSourceType(handle); }

    /** 来源描述, 如 "Wallpaper Engine (WorkerW 窗口独立捕获, 1920x1080, 纯壁纸)" */
    public String getSourceName() { return nativeGetSourceName(handle); }

    /** 近一秒内成功采集的帧数 (静态壁纸恒为 0/1) */
    public int getFps() { return nativeGetFps(handle); }

    @Override
    public void close() {
        if (handle != 0) {
            nativeDispose(handle);
            handle = 0;
            pixels = null;
        }
    }

    private native long nativeInit(int preferredSource);
    private native void nativeDispose(long handle);
    private native int nativeUpdate(long handle);
    private native ByteBuffer nativeGetPixels(long handle);
    private native int nativeGetWidth(long handle);
    private native int nativeGetHeight(long handle);
    private native int nativeGetSourceType(long handle);
    private native String nativeGetSourceName(long handle);
    private native int nativeGetFps(long handle);
    private static native String nativeLastError();
}
