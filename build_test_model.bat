@echo off
setlocal enabledelayedexpansion

rem ============================================================================
rem build_test_model.bat [model] - Compila y ejecuta JunoUnitTests para un modelo.
rem   model: 0=Super Six, 1=J106, 2=J60, 3=J6
rem Deteccion de VS/CMake en scripts\build_env.bat.
rem ============================================================================

call "%~dp0scripts\build_env.bat"
if errorlevel 1 exit /b 1

if "%~1"=="" (
    echo Usage: build_test_model.bat [model]
    echo   model: 0=SuperSix, 1=J106, 2=J60, 3=J6
    exit /b 1
)

set "MODEL=%~1"
set "BUILD_DIR=build_test_m%MODEL%"

if %MODEL%==0 set "MODEL_NAME=Super Six"
if %MODEL%==1 set "MODEL_NAME=JUNiO 601 (J-106)"
if %MODEL%==2 set "MODEL_NAME=JUNiO 06 (J-60)"
if %MODEL%==3 set "MODEL_NAME=JUNiO SIX (J-6)"

echo [INFO] VS:    %VS_PATH%
echo [INFO] CMake: %CMAKE_PATH%
call "%VC_VARS%" x64 >nul

echo ========================================================
echo Building JunoUnitTests - JUNO_TARGET_MODEL=%MODEL% (%MODEL_NAME%)
echo ========================================================

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

echo [Step 1] Configuring CMake...
"%CMAKE_PATH%" -S . -B "%BUILD_DIR%" -G "%VS_GENERATOR%" -A x64 -DCMAKE_GENERATOR_INSTANCE="%VS_INSTANCE%" -DJUCE_PATH="%JUCE_PATH%" -DJUNO_TARGET_MODEL=%MODEL%
if !ERRORLEVEL! NEQ 0 (
    echo [WARN] Configuracion fallida. Reintentando con --fresh...
    if exist "%BUILD_DIR%\_juce" rmdir /s /q "%BUILD_DIR%\_juce"
    "%CMAKE_PATH%" --fresh -S . -B "%BUILD_DIR%" -G "%VS_GENERATOR%" -A x64 -DCMAKE_GENERATOR_INSTANCE="%VS_INSTANCE%" -DJUCE_PATH="%JUCE_PATH%" -DJUNO_TARGET_MODEL=%MODEL%
    if !ERRORLEVEL! NEQ 0 (
        echo [ERROR] CMake configure failed!
        goto error
    )
)
echo [OK] CMake configured.

echo.
echo [Step 2] Building JunoUnitTests...
"%CMAKE_PATH%" --build "%BUILD_DIR%" --config Release --target JunoUnitTests
if !ERRORLEVEL! NEQ 0 (
    echo [ERROR] Build failed!
    goto error
)
echo [OK] Build succeeded.

echo.
echo [Step 3] Running JunoUnitTests...
echo.

set "TEST_EXE=%BUILD_DIR%\JunoUnitTests_artefacts\Release\JunoUnitTests.exe"
if not exist "!TEST_EXE!" (
    echo [ERROR] Test executable not found!
    dir /s /b "%BUILD_DIR%\JunoUnitTests.exe" 2>nul
    goto error
)

"%TEST_EXE%"
set "TEST_EXIT=!ERRORLEVEL!"

echo.
echo ========================================================
if !TEST_EXIT! EQU 0 (
    echo [SUCCESS] ALL TESTS PASSED (JUNO_TARGET_MODEL=%MODEL%)
    echo ========================================================
    exit /b 0
) else (
    echo [FAILURE] Tests failed with exit code !TEST_EXIT! (JUNO_TARGET_MODEL=%MODEL%)
    echo ========================================================
    pause
    exit /b !TEST_EXIT!
)

:error
echo.
echo [ERROR] An error occurred during the build/test pipeline.
pause
exit /b 1
