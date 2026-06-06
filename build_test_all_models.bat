@echo off
setlocal enabledelayedexpansion

set "VS_PATH=C:\Program Files\Microsoft Visual Studio\18\Community"
set "CMAKE_PATH=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

set OVERALL_EXIT=0

for %%M in (1 2 3) do (
    set MODEL=%%M
    set BUILD_DIR=build_test_m%%M
    
    if %%M==1 set "MODEL_NAME=JUNiO 601 (J-106)"
    if %%M==2 set "MODEL_NAME=JUNiO 06 (J-60)"
    if %%M==3 set "MODEL_NAME=JUNiO SIX (J-6)"
    
    echo.
    echo ========================================================
    echo [MODEL %%M] Building JunoUnitTests — !MODEL_NAME!
    echo ========================================================
    
    rem Clean build directory
    if exist "!BUILD_DIR!" rmdir /s /q "!BUILD_DIR!"
    
    echo.
    echo [Step 1] Configuring CMake with JUNO_TARGET_MODEL=%%M...
    call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64
    "%CMAKE_PATH%" -S . -B !BUILD_DIR! -G "Visual Studio 18 2026" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.26100.0 -DJUNO_TARGET_MODEL=%%M
    if !ERRORLEVEL! NEQ 0 (
        echo [ERROR] CMake configure failed for model %%M!
        set OVERALL_EXIT=1
        goto :continue
    )
    echo [OK] CMake configured.
    
    echo.
    echo [Step 2] Building JunoUnitTests...
    "%CMAKE_PATH%" --build !BUILD_DIR! --config Release --target JunoUnitTests
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
        dir /s /b !BUILD_DIR!\JunoUnitTests.exe 2>nul
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
