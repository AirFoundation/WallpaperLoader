// JNI 桥接: dev.yuxing.irotoridoriNoSekai.WallpaperLoader
#include <jni.h>
#include <new>
#include "wallpaper_loader.h"
#include "util.h"

namespace {
WallpaperLoaderCore* toCore(jlong handle) {
    return reinterpret_cast<WallpaperLoaderCore*>(handle);
}
} // namespace

extern "C" {

JNIEXPORT jlong JNICALL
Java_dev_yuxing_irotoridoriNoSekai_WallpaperLoader_nativeInit(JNIEnv*, jobject, jint preferred) {
    auto* core = new (std::nothrow) WallpaperLoaderCore();
    if (!core) { wl::LastError() = "内存分配失败"; return 0; }
    std::string err;
    if (!core->init(static_cast<int>(preferred), err)) {
        wl::LastError() = err;
        delete core;
        return 0;
    }
    return reinterpret_cast<jlong>(core);
}

JNIEXPORT void JNICALL
Java_dev_yuxing_irotoridoriNoSekai_WallpaperLoader_nativeDispose(JNIEnv*, jobject, jlong handle) {
    delete toCore(handle);
}

JNIEXPORT jint JNICALL
Java_dev_yuxing_irotoridoriNoSekai_WallpaperLoader_nativeUpdate(JNIEnv*, jobject, jlong handle) {
    auto* core = toCore(handle);
    if (!core) return 0;
    std::string err;
    int r = core->update(err);
    if (r == 0 && !err.empty()) wl::LastError() = err;
    return static_cast<jint>(r);
}

JNIEXPORT jobject JNICALL
Java_dev_yuxing_irotoridoriNoSekai_WallpaperLoader_nativeGetPixels(JNIEnv* env, jobject, jlong handle) {
    auto* core = toCore(handle);
    if (!core || core->frame().empty()) return nullptr;
    FrameBuffer& f = core->frame();
    return env->NewDirectByteBuffer(f.pixels.data(), static_cast<jlong>(f.byteSize()));
}

JNIEXPORT jint JNICALL
Java_dev_yuxing_irotoridoriNoSekai_WallpaperLoader_nativeGetWidth(JNIEnv*, jobject, jlong handle) {
    auto* core = toCore(handle);
    return core ? static_cast<jint>(core->width()) : 0;
}

JNIEXPORT jint JNICALL
Java_dev_yuxing_irotoridoriNoSekai_WallpaperLoader_nativeGetHeight(JNIEnv*, jobject, jlong handle) {
    auto* core = toCore(handle);
    return core ? static_cast<jint>(core->height()) : 0;
}

JNIEXPORT jint JNICALL
Java_dev_yuxing_irotoridoriNoSekai_WallpaperLoader_nativeGetSourceType(JNIEnv*, jobject, jlong handle) {
    auto* core = toCore(handle);
    return core ? static_cast<jint>(core->sourceType()) : 0;
}

JNIEXPORT jstring JNICALL
Java_dev_yuxing_irotoridoriNoSekai_WallpaperLoader_nativeGetSourceName(JNIEnv* env, jobject, jlong handle) {
    auto* core = toCore(handle);
    const std::string name = core ? core->sourceName() : std::string();
    return env->NewStringUTF(name.c_str());
}

JNIEXPORT jint JNICALL
Java_dev_yuxing_irotoridoriNoSekai_WallpaperLoader_nativeGetFps(JNIEnv*, jobject, jlong handle) {
    auto* core = toCore(handle);
    return core ? static_cast<jint>(core->fps()) : 0;
}

JNIEXPORT jstring JNICALL
Java_dev_yuxing_irotoridoriNoSekai_WallpaperLoader_nativeLastError(JNIEnv* env, jclass) {
    return env->NewStringUTF(wl::LastError().c_str());
}

} // extern "C"
