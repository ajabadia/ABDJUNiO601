@echo off
setlocal enabledelayedexpansion
echo ========================================
echo JUNiO 601 Improved Build
echo ========================================

:: 1. Setup Environment
set "VC_VARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
if exist "%VC_VARS%" (
    call "%VC_VARS%" x64
) else (
    echo [WARNING] vcvarsall.bat not found at %VC_VARS%
)

:: 2. Locate CMake
set "CMAKE_PATH=cmake"
set "POTENTIAL_CMAKE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if exist "%POTENTIAL_CMAKE%" (
    set "CMAKE_PATH=%POTENTIAL_CMAKE%"
    echo [INFO] Using CMake from VS 2026: !CMAKE_PATH!
)

:: 3. Build Version
set "VERSION_FILE=build_no.txt"
if not exist %VERSION_FILE% echo 0 > %VERSION_FILE%
set /p build_no=<%VERSION_FILE%
set /a build_no=%build_no% + 1
echo %build_no% > %VERSION_FILE%

echo #define JUNO_BUILD_VERSION "%build_no%" > "Source/Core/BuildVersion.h"
echo #define JUNO_BUILD_TIMESTAMP "%DATE% %TIME%" >> "Source/Core/BuildVersion.h"

:: 4. Build
set "BUILD_DIR=build"
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

echo [INFO] Configuring...
"%CMAKE_PATH%" -B %BUILD_DIR% -A x64 -T v143 -DBUILD_SHARED_LIBS=OFF
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

echo [INFO] Building targets...
"%CMAKE_PATH%" --build %BUILD_DIR% --config Release --target ABDSimpleJuno106_Standalone
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Standalone build failed!
    exit /b %ERRORLEVEL%
)

"%CMAKE_PATH%" --build %BUILD_DIR% --config Release --target JunoUnitTests
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Unit Tests build failed!
    exit /b %ERRORLEVEL%
)

echo ========================================
echo BUILD SUCCESSFUL!
echo ========================================
