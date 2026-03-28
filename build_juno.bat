@echo off
setlocal enabledelayedexpansion

echo ========================================
echo JUNiO 601 Improved Build
echo ========================================

set "VC_VARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"

if exist "%VC_VARS%" (
    call "%VC_VARS%" x64
) else (
    echo [WARNING] vcvarsall.bat not found at %VC_VARS%
)

set "CMAKE_PATH=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if not exist build mkdir build

echo [INFO] Configuring...
"%CMAKE_PATH%" -S . -B build -G "Visual Studio 18 2026" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.26100.0

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed
    exit /b %ERRORLEVEL%
)

echo [INFO] Building targets...
"%CMAKE_PATH%" --build build --config Release --target ABDSimpleJuno106_Standalone

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Standalone build failed
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] Build completed.
exit /b 0
