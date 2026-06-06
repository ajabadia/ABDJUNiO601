@echo off
setlocal enabledelayedexpansion

set "VC_VARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
set "CMAKE_PATH=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if exist "%VC_VARS%" (
    call "%VC_VARS%" x64
) else (
    echo [WARNING] vcvarsall.bat not found
)

set BUILD_DIR=build_j6
set TARGET_MODEL=3

if not exist %BUILD_DIR% mkdir %BUILD_DIR%

echo [INFO] Configuring JUNO_TARGET_MODEL=%TARGET_MODEL%...
"%CMAKE_PATH%" -S . -B %BUILD_DIR% -G "Visual Studio 18 2026" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.26100.0 -DJUNO_TARGET_MODEL=%TARGET_MODEL%
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed with code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo [INFO] Building...
"%CMAKE_PATH%" --build %BUILD_DIR% --config Release --target ABDSimpleJuno106_Standalone
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed with code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] Build completed.
exit /b 0
