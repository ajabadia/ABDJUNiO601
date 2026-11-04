@echo off
setlocal enabledelayedexpansion

rem ============================================================================
rem build_native_ui.bat - Build de verificacion con la UI nativa (USE_NATIVE_UI=ON).
rem Deteccion de VS/CMake en scripts\build_env.bat.
rem ============================================================================

call "%~dp0scripts\build_env.bat"
if errorlevel 1 exit /b 1

echo ========================================
echo JUNiO 601 NATIVE UI Build (Verification)
echo ========================================
echo [INFO] VS:    %VS_PATH%
echo [INFO] CMake: %CMAKE_PATH%
echo.

call "%VC_VARS%" x64 >nul

rem --- Incrementar version de Build (compartida) ---
set "VERSION_FILE=build_no.txt"
if not exist %VERSION_FILE% echo 0 > %VERSION_FILE%
set /p build_no=<%VERSION_FILE%
set /a build_no=build_no + 1
echo %build_no% > %VERSION_FILE%

echo #define JUNO_BUILD_VERSION "%build_no%" > "Source/Core/BuildVersion.h"
echo #define JUNO_BUILD_TIMESTAMP "%DATE% %TIME%" >> "Source/Core/BuildVersion.h"

rem --- Configurar y compilar (con flag de Native UI) ---
set BUILD_DIR=build_native
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

echo.
echo 1. Configurando proyecto (Native UI)...
"%CMAKE_PATH%" -S . -B %BUILD_DIR% -G "%VS_GENERATOR%" -A x64 -DCMAKE_GENERATOR_INSTANCE="%VS_INSTANCE%" -DJUCE_PATH="%JUCE_PATH%" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DBUILD_SHARED_LIBS=OFF -DUSE_NATIVE_UI=ON
if !ERRORLEVEL! NEQ 0 (
    echo [WARN] Configuracion fallida; reintentando con --fresh...
    if exist "%BUILD_DIR%\_juce" rmdir /s /q "%BUILD_DIR%\_juce"
    "%CMAKE_PATH%" --fresh -S . -B %BUILD_DIR% -G "%VS_GENERATOR%" -A x64 -DCMAKE_GENERATOR_INSTANCE="%VS_INSTANCE%" -DJUCE_PATH="%JUCE_PATH%" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DBUILD_SHARED_LIBS=OFF -DUSE_NATIVE_UI=ON
    if !ERRORLEVEL! NEQ 0 (
        echo [ERROR] Error en la configuracion de CMake.
        exit /b 1
    )
)

echo.
echo 2. Compilando JUNiO 601 Native (Release)...
"%CMAKE_PATH%" --build %BUILD_DIR% --config Release --target ABDSimpleJuno106_Standalone
if !ERRORLEVEL! NEQ 0 (
    echo [ERROR] La compilacion ha fallado.
    exit /b 1
)

rem --- Mover ejecutable ---
echo.
echo 3. Finalizando...
set EXE_SRC=%BUILD_DIR%\ABDSimpleJuno106_artefacts\Release\Standalone\ABDSimpleJuno106.exe
if not exist "%EXE_SRC%" set EXE_SRC=%BUILD_DIR%\Release\ABDSimpleJuno106.exe

if exist "%EXE_SRC%" (
    copy /Y "%EXE_SRC%" "JUNiO_601_Native.exe" >nul
    echo.
    echo ========================================
    echo BUILD NATIVA EXITOSA! (Build #%build_no%)
    echo Ejecutable: JUNiO_601_Native.exe
    echo ========================================
) else (
    echo [ADVERTENCIA] No se encontro el .exe compilado, revisa la carpeta build_native.
)
