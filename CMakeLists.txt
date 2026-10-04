cmake_minimum_required(VERSION 3.20)
project(WallpaperLoader CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 需要 JDK (提供 jni.h)。FindJNI 会自动使用 JAVA_HOME 环境变量。
find_package(JNI REQUIRED)

add_library(WallpaperLoader SHARED
    native/jni_bridge.cpp
    native/core.cpp
    native/util.cpp
    native/source_static.cpp
    native/source_dxgi.cpp
)

# Wallpaper Engine 独立窗口捕获依赖 C++/WinRT, 仅 MSVC + Windows SDK 可用
if(MSVC)
    target_sources(WallpaperLoader PRIVATE native/source_we_graphics.cpp)
    target_compile_definitions(WallpaperLoader PRIVATE WALLPAPER_HAS_WE_GRAPHICS=1)
    message(STATUS "Wallpaper Engine 窗口独立捕获: 已启用 (MSVC)")
else()
    message(STATUS "Wallpaper Engine 窗口独立捕获: 未启用 (需要 MSVC, 将回退到 DXGI 桌面复制)")
endif()

target_include_directories(WallpaperLoader PRIVATE
    ${JNI_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/native
)

if(MSVC)
    target_compile_options(WallpaperLoader PRIVATE /W3 /EHsc)
    target_compile_definitions(WallpaperLoader PRIVATE NOMINMAX)
    # d3d11/dxgi: DXGI 桌面复制 ; windowscodecs: WIC 壁纸解码 ; windowsapp: WinRT 捕获
    target_link_libraries(WallpaperLoader PRIVATE
        d3d11 dxgi windowscodecs windowsapp ${JNI_LIBRARIES})
endif()

set_target_properties(WallpaperLoader PROPERTIES
    OUTPUT_NAME "WallpaperLoader"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
)
