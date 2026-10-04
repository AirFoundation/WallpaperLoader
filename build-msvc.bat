@echo off
rem Build WallpaperLoader.dll (x64). English-only to avoid cmd codepage issues.
rem Prerequisites: Visual Studio 2022 (or Build Tools) with C++ workload,
rem                CMake, JDK 17+, JAVA_HOME pointing at a JDK.
rem CMake auto-detects Visual Studio; no manual vcvarsall needed.
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

cmake -S "%~dp0" -B "%~dp0build" -A x64
if errorlevel 1 (
  echo [ERROR] CMake configure failed. Most likely causes:
  echo         - No Visual Studio with C++ workload installed
  echo           (install VS 2022 Build Tools + "Desktop development with C++")
  echo         - No JDK found via JAVA_HOME
  exit /b 1
)

cmake --build "%~dp0build" --config Release
if errorlevel 1 (
  echo [ERROR] Build failed. Paste the full output for help.
  exit /b 1
)

echo.
echo [DONE] %~dp0build\Release\WallpaperLoader.dll
endlocal
