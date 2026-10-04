@echo off
rem 一键构建 WallpaperLoader.dll (x64)
rem 前置: Visual Studio 2022 Build Tools (含 C++ 桌面开发 + Windows SDK),
rem       CMake, JDK 17+, 并设置 JAVA_HOME 环境变量。
chcp 65001 >nul
setlocal

where cmake >nul 2>nul || (echo [错误] 未找到 cmake, 请先安装 & exit /b 1)
if not defined JAVA_HOME (echo [错误] 请先设置 JAVA_HOME 指向 JDK 根目录 & exit /b 1)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo [错误] 未找到 vswhere.exe, 请安装 VS Build Tools & exit /b 1)

for /f "usebackq tokens=*" %%i in (
  `"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`
) do set "VSINSTALL=%%i"
if not defined VSINSTALL (echo [错误] 未找到含 C++ 工具集的 VS 安装 & exit /b 1)

call "%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" x64 || exit /b 1

cmake -S "%~dp0" -B "%~dp0build" -A x64 || exit /b 1
cmake --build "%~dp0build" --config Release || exit /b 1

echo.
echo [完成] 产物: %~dp0build\Release\WallpaperLoader.dll
endlocal
