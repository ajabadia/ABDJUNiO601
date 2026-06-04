@echo off
setlocal enabledelayedexpansion

:: ============================================================================
:: build_and_test.bat - CI Pipeline: Build + Launch + CDP Test + Cleanup
:: ============================================================================
:: Usage:
::   build_and_test.bat              Full pipeline (build + test)
::   build_and_test.bat --test-only  Skip build, just launch + test
::   build_and_test.bat --help       Show this help
:: ============================================================================

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"
set "APP_NAME=ABD JUNiO 601.exe"
set "APP_REL_DIR=%BUILD_DIR%\ABDSimpleJuno106_artefacts\Release\Standalone"
set "APP_PATH=%APP_REL_DIR%\%APP_NAME%"
set "CDP_PORT=9222"
set "EXIT_CODE=0"

:: Detect CMake path
set "CMAKE="
if exist "%SCRIPT_DIR%CMake\CMake\bin\cmake.exe" (
    set "CMAKE=%SCRIPT_DIR%CMake\CMake\bin\cmake.exe"
) else if exist "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
    set "CMAKE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
) else (
    where cmake >nul 2>&1 && set "CMAKE=cmake" || (
        echo [CI] ERROR: CMake not found
        exit /b 1
    )
)
echo [CI] Using CMake: %CMAKE%

:: Parse args
set "SKIP_BUILD="
if /i "%1"=="--test-only" set "SKIP_BUILD=1"
if /i "%1"=="--help" (
    echo Usage: %~nx0 [--test-only] [--help]
    echo   --test-only  Skip the build step, just launch and test
    echo   --help       Show this message
    exit /b 0
)

echo.
echo ============================================================================
echo  CI PIPELINE: Build - Launch - CDP Test - Cleanup
echo ============================================================================

:: -- 1. BUILD ---------------------------------------------------------------
if defined SKIP_BUILD (
    echo.
    echo [CI] Skipping build (--test-only)
) else (
    echo.
    echo [CI] Step 1/5: Building standalone app...
    echo.
    if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" (
        call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
    )
    "%CMAKE%" --build "%BUILD_DIR%" --config Release --target ABDSimpleJuno106_Standalone
    if !ERRORLEVEL! neq 0 (
        echo [CI] BUILD FAILED (exit code: !ERRORLEVEL!)
        exit /b !ERRORLEVEL!
    )
    echo [CI] Build OK
)

:: -- 2. KILL OLD PROCESS ----------------------------------------------------
echo.
echo [CI] Step 2/5: Killing any existing app instance...
taskkill /F /IM "%APP_NAME%" 2>nul
echo [CI] Done

:: -- 3. LAUNCH WITH CDP -----------------------------------------------------
echo.
echo [CI] Step 3/5: Launching app with CDP on port %CDP_PORT%...
set "WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=--remote-debugging-port=%CDP_PORT% --remote-allow-origins=*"
start "" /B "%APP_PATH%" >nul 2>&1
echo [CI] App launched, waiting for CDP...

set "CDP_WAIT=0"
:wait_cdp
set /a CDP_WAIT+=1
timeout /t 2 /nobreak >nul
netstat -an 2>&1 | findstr "LISTENING" | findstr ":%CDP_PORT%" >nul
if errorlevel 1 (
    if !CDP_WAIT! lss 15 goto wait_cdp
    echo [CI] ERROR: CDP port %CDP_PORT% not ready after 30s
    taskkill /F /IM "%APP_NAME%" 2>nul
    exit /b 1
)
echo [CI] CDP ready after !CDP_WAIT! polling cycles

:: -- 4. RUN TESTS -----------------------------------------------------------
echo.
echo [CI] Step 4/5: Running regression tests...
echo.

echo [TEST] test_smart_import_all_formats.py (55 checks)
python -X utf8 "%SCRIPT_DIR%scripts\test_smart_import_all_formats.py"
set "EXIT_CODE=!ERRORLEVEL!"
if !EXIT_CODE! neq 0 (
    echo [TEST] FAILED: test_smart_import_all_formats.py
) else (
    echo [TEST] PASSED: 55/55 all formats
    echo.
    echo [TEST] test_import_real.py (16 checks, fallback mode)
    python -X utf8 "%SCRIPT_DIR%scripts\test_import_real.py"
    if !ERRORLEVEL! neq 0 (
        echo [TEST] FAILED: test_import_real.py
        set "EXIT_CODE=!ERRORLEVEL!"
    ) else (
        echo [TEST] PASSED: 16/16 real import
    )
)
echo.

if !EXIT_CODE! equ 0 (
    echo [CI] ALL TESTS PASSED
) else (
    echo [CI] TEST FAILED (exit code: !EXIT_CODE!)
)

:: -- 5. CLEANUP -------------------------------------------------------------
echo.
echo [CI] Step 5/5: Cleanup...
taskkill /F /IM "%APP_NAME%" 2>nul
echo [CI] App terminated

echo.
echo ============================================================================
if !EXIT_CODE! equ 0 (
    echo  CI PIPELINE: ALL PASSED
) else (
    echo  CI PIPELINE: FAILED (exit code: !EXIT_CODE!)
)
echo ============================================================================

exit /b !EXIT_CODE!
