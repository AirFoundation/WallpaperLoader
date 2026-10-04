@echo off
rem Build WallpaperLoader.dll (x64). English-only to avoid cmd codepage issues.
rem Prerequisites: VS 2022 Build Tools (C++ desktop workload + Windows SDK),
rem                CMake, JDK 17+, JAVA_HOME pointing at a JDK.
setlocal

where cmake >nul 2>nul
if errorlevel 1 (
  echo [ERROR] cmake not found on PATH.
  echo         Install it:  winget install Kitware.CMake
  echo         Then close and reopen this terminal.
  exit /b 1
)

if not defined JAVA_HOME (
  echo [ERROR] JAVA_HOME is not defined in this terminal.
  echo         Set it to your JDK root, e.g.:
  echo           setx JAVA_HOME "C:\Program Files\Java\jdk-17"
  echo         Then close and reopen this terminal.
  exit /b 1
)
echo [INFO] JAVA_HOME=%JAVA_HOME%
if not exist "%JAVA_HOME%\include\jni.h" (
  echo [ERROR] %%JAVA_HOME%%\include\jni.h not found.
  echo         JAVA_HOME must point at a JDK (not a JRE): %JAVA_HOME%
  exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo [ERROR] vswhere.exe not found. Install VS 2022 Build Tools with the
  echo         "Desktop development with C++" workload.
  exit /b 1
)

for /f "usebackq tokens=*" %%i in (
  `"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`
) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
  echo [ERROR] No VS installation with the C++ x64 toolset found.
  exit /b 1
)

call "%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" x64 || exit /b 1

cmake -S "%~dp0" -B "%~dp0build" -A x64 || exit /b 1
cmake --build "%~dp0build" --config Release || exit /b 1

echo.
echo [DONE] %~dp0build\Release\WallpaperLoader.dll
endlocal
