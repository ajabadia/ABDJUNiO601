@echo off
setlocal enabledelayedexpansion

set "VS_PATH=C:\Program Files\Microsoft Visual Studio\18\Community"
set "CMAKE_PATH=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if "%1"=="" (
    echo Usage: build_test_model.bat [model]
    echo   model: 0=SuperSix, 1=J106, 2=J60, 3=J6
    exit /b 1
)

set MODEL=%1
set BUILD_DIR=build_test_m%MODEL%

if %MODEL%==0 set MODEL_NAME=Super Six
if %MODEL%==1 set MODEL_NAME=JUNiO 601 (J-106)
if %MODEL%==2 set MODEL_NAME=JUNiO 06 (J-60)
if %MODEL%==3 set MODEL_NAME=JUNiO SIX (J-6)

echo ========================================================
echo Building JunoUnitTests — JUNO_TARGET_MODEL=%MODEL% (%MODEL_NAME%)
echo ========================================================

rem Clean build directory
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

echo [Step 1] Configuring CMake...
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64
"%CMAKE_PATH%" -S . -B %BUILD_DIR% -G "Visual Studio 18 2026" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.26100.0 -DJUNO_TARGET_MODEL=%MODEL%
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
set TEST_EXIT=%ERRORLEVEL%

echo.
echo ========================================================
if %TEST_EXIT% EQU 0 (
    echo [SUCCESS] ALL TESTS PASSED (JUNO_TARGET_MODEL=%MODEL%)
) else (
    echo [FAILURE] Tests failed with exit code %TEST_EXIT% (JUNO_TARGET_MODEL=%MODEL%)
)
echo ========================================================
exit /b %TEST_EXIT%
