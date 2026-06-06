@echo off
setlocal enabledelayedexpansion

set "VS_PATH=C:\Program Files\Microsoft Visual Studio\18\Community"
set "CMAKE_PATH=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "BUILD_DIR=build_test_j106"
set TARGET_MODEL=1

echo ========================================================
echo Building JunoUnitTests — JUNO_TARGET_MODEL=%TARGET_MODEL% (JUNiO 601)
echo ========================================================

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

echo [Step 1] Configuring CMake...
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64
"%CMAKE_PATH%" -S . -B %BUILD_DIR% -G "Visual Studio 18 2026" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.26100.0 -DJUNO_TARGET_MODEL=%TARGET_MODEL%
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configure failed!
    exit /b 1
)
echo [OK] CMake configured.

echo.
echo [Step 2] Building JunoUnitTests...
"%CMAKE_PATH%" --build %BUILD_DIR% --config Release --target JunoUnitTests
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    exit /b 1
)
echo [OK] Build succeeded.

echo.
echo [Step 3] Running JunoUnitTests...
echo.

set "TEST_EXE=%BUILD_DIR%\JunoUnitTests_artefacts\Release\JunoUnitTests.exe"
if not exist "%TEST_EXE%" (
    echo [ERROR] Test executable not found!
    dir /s /b %BUILD_DIR%\JunoUnitTests.exe 2>nul
    exit /b 1
)

"%TEST_EXE%"
set "TEST_EXIT=%ERRORLEVEL%"

echo.
if %TEST_EXIT% EQU 0 (
    echo [SUCCESS] ALL TESTS PASSED (JUNO_TARGET_MODEL=%TARGET_MODEL%)
) else (
    echo [FAILURE] Tests failed (exit code: %TEST_EXIT%)
)
exit /b %TEST_EXIT%
