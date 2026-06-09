@echo off
setlocal enabledelayedexpansion

rem ============================================================================
rem build_model.bat — Build JUNiO 601 for a specific model
rem
rem Usage:
rem   build_model.bat [model] [build_dir]
rem
rem   model      0=Super Six (default), 1=Juno-106, 2=Juno-60, 3=Juno-6
rem   build_dir  Optional output directory (default: build_<model_name>)
rem
rem Examples:
rem   build_model.bat          → Super Six (build_supersix)
rem   build_model.bat 1        → Juno-106 (build_j106)
rem   build_model.bat 2 mydir  → Juno-60 (mydir)
rem ============================================================================

set "VC_VARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
set "CMAKE_PATH=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if exist "%VC_VARS%" (
    call "%VC_VARS%" x64
) else (
    echo [WARNING] vcvarsall.bat not found at %VC_VARS%
)

if not exist "%CMAKE_PATH%" (
    echo [ERROR] CMake not found at %CMAKE_PATH%
    goto error
)

rem --- Parse arguments ---
set MODEL=0
if not "%1"=="" set MODEL=%1

if "%2"=="" (
    if %MODEL%==0 set "BUILD_DIR=build_supersix"
    if %MODEL%==1 set "BUILD_DIR=build_j106"
    if %MODEL%==2 set "BUILD_DIR=build_j60"
    if %MODEL%==3 set "BUILD_DIR=build_j6"
) else (
    set BUILD_DIR=%2
)

rem --- Resolve model name for display ---
if %MODEL%==0 set "MODEL_NAME=Super Six (Hybrid)"
if %MODEL%==1 set "MODEL_NAME=JUNiO 601 (Juno-106)"
if %MODEL%==2 set "MODEL_NAME=JUNiO 06 (Juno-60)"
if %MODEL%==3 set "MODEL_NAME=JUNiO SIX (Juno-6)"

echo ========================================
echo Building: %MODEL_NAME%
echo JUNO_TARGET_MODEL=%MODEL%
echo Build dir: %BUILD_DIR%
echo ========================================

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem --- Increment build number ---
set "VERSION_FILE=build_no.txt"
if not exist %VERSION_FILE% echo 0 > %VERSION_FILE%
set /p build_no=<%VERSION_FILE%
set /a build_no=%build_no% + 1
echo %build_no% > %VERSION_FILE%

echo #define JUNO_BUILD_VERSION "%build_no%" > "Source/Core/BuildVersion.h"
echo #define JUNO_BUILD_TIMESTAMP "%DATE% %TIME%" >> "Source/Core/BuildVersion.h"

echo [INFO] Configuring...
"%CMAKE_PATH%" -S . -B "%BUILD_DIR%" -G "Visual Studio 18 2026" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.26100.0 -DJUNO_TARGET_MODEL=%MODEL%
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed with code %ERRORLEVEL%
    goto error
)

echo [INFO] Building standalone...
"%CMAKE_PATH%" --build "%BUILD_DIR%" --config Release --target ABDSimpleJuno106_Standalone
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed with code %ERRORLEVEL%
    goto error
)

echo [SUCCESS] %MODEL_NAME% built successfully.
exit /b 0

:error
echo.
echo [ERROR] Build failed.
pause
exit /b 1
