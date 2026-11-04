@echo off
setlocal enabledelayedexpansion

rem ============================================================================
rem build_tests.bat - Compila y ejecuta JunoUnitTests con el modelo Super Six.
rem   build_tests.bat            compila y ejecuta
rem   build_tests.bat --clean    borra build_supersix antes de compilar
rem La deteccion de Visual Studio / CMake vive en scripts\build_env.bat.
rem ============================================================================

call "%~dp0scripts\build_env.bat"
if errorlevel 1 exit /b 1

echo [INFO] VS:    %VS_PATH%
echo [INFO] CMake: %CMAKE_PATH%
call "%VC_VARS%" x64 >nul

set "BUILD_DIR=build_supersix"
set "MODEL=0"

if /i "%~1"=="--clean" (
    if exist "%BUILD_DIR%" (
        echo [INFO] Flag --clean detectado. Eliminando %BUILD_DIR%...
        rmdir /s /q "%BUILD_DIR%"
    )
)

echo.
echo === Building JunoUnitTests (Super Six model, JUNO_TARGET_MODEL=%MODEL%) ===
"%CMAKE_PATH%" -S . -B "%BUILD_DIR%" -G "%VS_GENERATOR%" -A x64 -DCMAKE_GENERATOR_INSTANCE="%VS_INSTANCE%" -DJUCE_PATH="%JUCE_PATH%" -DJUNO_TARGET_MODEL=%MODEL%
if !ERRORLEVEL! NEQ 0 (
    echo [WARN] Configuracion fallida ^(cache de otra instalacion de VS?^). Reintentando con --fresh...
    if exist "%BUILD_DIR%\_juce" rmdir /s /q "%BUILD_DIR%\_juce"
    "%CMAKE_PATH%" --fresh -S . -B "%BUILD_DIR%" -G "%VS_GENERATOR%" -A x64 -DCMAKE_GENERATOR_INSTANCE="%VS_INSTANCE%" -DJUCE_PATH="%JUCE_PATH%" -DJUNO_TARGET_MODEL=%MODEL%
    if !ERRORLEVEL! NEQ 0 (
        echo [ERROR] CMake configuration failed!
        exit /b 1
    )
)

"%CMAKE_PATH%" --build "%BUILD_DIR%" --config Release --target JunoUnitTests
if !ERRORLEVEL! NEQ 0 (
    echo [ERROR] Build failed!
    exit /b 1
)
echo [SUCCESS] Build OK.
echo.

echo === Running JunoUnitTests ===
set "TEST_EXE=%BUILD_DIR%\JunoUnitTests_artefacts\Release\JunoUnitTests.exe"
if not exist "!TEST_EXE!" (
    echo [ERROR] No se encontro !TEST_EXE!
    dir /s /b "%BUILD_DIR%\JunoUnitTests.exe" 2>nul
    exit /b 1
)

"%TEST_EXE%"
set "TEST_EXIT=!ERRORLEVEL!"

echo.
if !TEST_EXIT! EQU 0 (
    echo [SUCCESS] ALL TESTS PASSED
) else (
    echo [FAILURE] Tests failed ^(exit code: !TEST_EXIT!^)
)
echo === Done ===
exit /b !TEST_EXIT!
