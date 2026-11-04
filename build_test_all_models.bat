@echo off
setlocal enabledelayedexpansion

rem ============================================================================
rem build_test_all_models.bat - Compila y ejecuta JunoUnitTests para 1, 2 y 3
rem (Super Six se cubre con build_tests.bat).
rem Deteccion de VS/CMake en scripts\build_env.bat.
rem ============================================================================

call "%~dp0scripts\build_env.bat"
if errorlevel 1 exit /b 1

echo [INFO] VS:    %VS_PATH%
echo [INFO] CMake: %CMAKE_PATH%
call "%VC_VARS%" x64 >nul

set OVERALL_EXIT=0

for %%M in (1 2 3) do (
    set MODEL=%%M
    set BUILD_DIR=build_test_m%%M

    if %%M==1 set "MODEL_NAME=JUNiO 601 (J-106)"
    if %%M==2 set "MODEL_NAME=JUNiO 06 (J-60)"
    if %%M==3 set "MODEL_NAME=JUNiO SIX (J-6)"

    echo.
    echo ========================================================
    echo [MODEL %%M] Building JunoUnitTests - !MODEL_NAME!
    echo ========================================================

    rem Clean build directory
    if exist "!BUILD_DIR!" rmdir /s /q "!BUILD_DIR!"

    echo.
    echo [Step 1] Configuring CMake with JUNO_TARGET_MODEL=%%M...
    "%CMAKE_PATH%" -S . -B "!BUILD_DIR!" -G "%VS_GENERATOR%" -A x64 -DCMAKE_GENERATOR_INSTANCE="%VS_INSTANCE%" -DJUCE_PATH="%JUCE_PATH%" -DJUNO_TARGET_MODEL=%%M
    if !ERRORLEVEL! NEQ 0 (
        echo [WARN] Configuracion fallida; reintentando con --fresh...
        if exist "!BUILD_DIR!\_juce" rmdir /s /q "!BUILD_DIR!\_juce"
        "%CMAKE_PATH%" --fresh -S . -B "!BUILD_DIR!" -G "%VS_GENERATOR%" -A x64 -DCMAKE_GENERATOR_INSTANCE="%VS_INSTANCE%" -DJUCE_PATH="%JUCE_PATH%" -DJUNO_TARGET_MODEL=%%M
        if !ERRORLEVEL! NEQ 0 (
            echo [ERROR] CMake configure failed for model %%M!
            set OVERALL_EXIT=1
            goto :continue
        )
    )
    echo [OK] CMake configured.

    echo.
    echo [Step 2] Building JunoUnitTests...
    "%CMAKE_PATH%" --build "!BUILD_DIR!" --config Release --target JunoUnitTests
    if !ERRORLEVEL! NEQ 0 (
        echo [ERROR] Build failed for model %%M!
        set OVERALL_EXIT=1
        goto :continue
    )
    echo [OK] Build succeeded.

    echo.
    echo [Step 3] Running JunoUnitTests for model %%M...
    echo.

    set "TEST_EXE=!BUILD_DIR!\JunoUnitTests_artefacts\Release\JunoUnitTests.exe"
    if not exist "!TEST_EXE!" (
        echo [ERROR] Test executable not found for model %%M!
        dir /s /b "!BUILD_DIR!\JunoUnitTests.exe" 2>nul
        set OVERALL_EXIT=1
        goto :continue
    )

    "!TEST_EXE!"
    if !ERRORLEVEL! NEQ 0 (
        echo [FAILURE] Tests failed for model %%M (exit code: !ERRORLEVEL!)
        set OVERALL_EXIT=1
    ) else (
        echo [SUCCESS] ALL TESTS PASSED for model %%M
    )

    :continue
    echo.
)

echo.
echo ========================================================
if !OVERALL_EXIT! EQU 0 (
    echo [SUCCESS] ALL MODELS PASSED
) else (
    echo [FAILURE] SOME MODELS HAD ERRORS
)
echo ========================================================
exit /b !OVERALL_EXIT!
