@echo off
setlocal enabledelayedexpansion

rem ============================================================================
rem build_test_j106.bat - Compila y ejecuta JunoUnitTests con JUNO_TARGET_MODEL=1
rem (JUNiO 601 / Juno-106). Deteccion de VS/CMake en scripts\build_env.bat.
rem ============================================================================

call "%~dp0scripts\build_env.bat"
if errorlevel 1 exit /b 1

set "BUILD_DIR=build_test_j106"
set "TARGET_MODEL=1"

echo [INFO] VS:    %VS_PATH%
echo [INFO] CMake: %CMAKE_PATH%
call "%VC_VARS%" x64 >nul

echo ========================================================
echo Building JunoUnitTests - JUNO_TARGET_MODEL=%TARGET_MODEL% (JUNiO 601)
echo ========================================================

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

echo [Step 1] Configuring CMake...
"%CMAKE_PATH%" -S . -B "%BUILD_DIR%" -G "%VS_GENERATOR%" -A x64 -DCMAKE_GENERATOR_INSTANCE="%VS_INSTANCE%" -DJUCE_PATH="%JUCE_PATH%" -DJUNO_TARGET_MODEL=%TARGET_MODEL%
if !ERRORLEVEL! NEQ 0 (
    echo [WARN] Configuracion fallida. Reintentando con --fresh...
    if exist "%BUILD_DIR%\_juce" rmdir /s /q "%BUILD_DIR%\_juce"
    "%CMAKE_PATH%" --fresh -S . -B "%BUILD_DIR%" -G "%VS_GENERATOR%" -A x64 -DCMAKE_GENERATOR_INSTANCE="%VS_INSTANCE%" -DJUCE_PATH="%JUCE_PATH%" -DJUNO_TARGET_MODEL=%TARGET_MODEL%
    if !ERRORLEVEL! NEQ 0 (
        echo [ERROR] CMake configure failed!
        exit /b 1
    )
)
echo [OK] CMake configured.

echo.
echo [Step 2] Building JunoUnitTests...
"%CMAKE_PATH%" --build "%BUILD_DIR%" --config Release --target JunoUnitTests
if !ERRORLEVEL! NEQ 0 (
    echo [ERROR] Build failed!
    exit /b 1
)
echo [OK] Build succeeded.

echo.
echo [Step 3] Running JunoUnitTests...
echo.

set "TEST_EXE=%BUILD_DIR%\JunoUnitTests_artefacts\Release\JunoUnitTests.exe"
if not exist "!TEST_EXE!" (
    echo [ERROR] Test executable not found!
    dir /s /b "%BUILD_DIR%\JunoUnitTests.exe" 2>nul
    exit /b 1
)

"%TEST_EXE%"
set "TEST_EXIT=!ERRORLEVEL!"

echo.
if !TEST_EXIT! EQU 0 (
    echo [SUCCESS] ALL TESTS PASSED (JUNO_TARGET_MODEL=%TARGET_MODEL%)
) else (
    echo [FAILURE] Tests failed (exit code: !TEST_EXIT!)
)
exit /b !TEST_EXIT!
