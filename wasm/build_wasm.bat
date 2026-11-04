@echo off
setlocal enabledelayedexpansion

rem ============================================================================
rem build_wasm.bat - Compila el motor DSP de ABDJUNiO601 a WebAssembly
rem ----------------------------------------------------------------------------
rem Usa scripts\build_env.bat para localizar el Emscripten SDK (D:\desarrollos
rem \emsdk) y los modulos JUCE (D:\desarrollos\juce). Los modulos se compilan
rem directamente desde el SDK: el repo ya no guarda copias de juce_core,
rem juce_dsp ni juce_audio_basics.
rem Salida: Source\UI\WebUI\wasm\abdjunio601_wasm.js (+ .wasm embebido).
rem ============================================================================

cd /d "%~dp0.."

call "scripts\build_env.bat"
if errorlevel 1 exit /b 1

if not defined JUCE_PATH (
    echo [ERROR] JUCE no encontrado ^(se esperaba D:\desarrollos\juce o JUCE_PATH^).
    exit /b 1
)

echo ========================================
echo  ABDJUNiO601 WASM Build
echo ========================================
echo [INFO] JUCE:  %JUCE_PATH%
echo [INFO] CMake: %CMAKE_PATH%
if defined EMSDK_PATH echo [INFO] EMSDK: %EMSDK_PATH%

rem --- 1. Emscripten en PATH --------------------------------------------------
rem    Quirk conocido (ver ABDMS2000\wasm\build_wasm.bat): llamar emsdk_env.bat
rem    con la salida silenciada tambien silencia su "set PATH", asi que se
rem    comprueba emcmake y, si sigue faltando, se monta el PATH a mano.
where emcmake >nul 2>&1
if errorlevel 1 if defined EMSDK_ENV call "%EMSDK_ENV%" >nul 2>&1
where emcmake >nul 2>&1
if errorlevel 1 if defined EMSDK_PATH (
    echo [INFO] Montando PATH de Emscripten a mano desde %EMSDK_PATH%...
    set "PATH=%EMSDK_PATH%;%EMSDK_PATH%\upstream\emscripten;%PATH%"
    if exist "%EMSDK_PATH%\.emscripten" set "EM_CONFIG=%EMSDK_PATH%\.emscripten"
)
where emcmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] emcmake no encontrado. Activa el emsdk ^(%EMSDK_ENV%^) o revisa D:\desarrollos\emsdk.
    exit /b 1
)

rem --- 2. Configurar y compilar -----------------------------------------------
if not exist "wasm\build" mkdir "wasm\build"

echo [1/2] Configuring CMake with Emscripten (Ninja)...
call emcmake cmake -S wasm -B wasm\build -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_MODULES_DIR="%JUCE_PATH%\modules"
if !ERRORLEVEL! NEQ 0 (
    echo [WARN] Configuracion fallida ^(cache de otro emsdk o de otra copia de JUCE?^). Reintentando con --fresh...
    if exist "wasm\build\CMakeCache.txt" del /q "wasm\build\CMakeCache.txt"
    call emcmake cmake --fresh -S wasm -B wasm\build -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_MODULES_DIR="%JUCE_PATH%\modules"
    if !ERRORLEVEL! NEQ 0 (
        echo [ERROR] CMake configuration failed!
        exit /b 1
    )
)

echo [2/2] Building WASM module...
"%CMAKE_PATH%" --build wasm\build
if !ERRORLEVEL! NEQ 0 (
    echo [ERROR] Build failed!
    exit /b 1
)

echo.
echo ========================================
echo  Build complete!
echo  Output: Source\UI\WebUI\wasm\abdjunio601_wasm.js
echo          Source\UI\WebUI\wasm\abdjunio601_wasm.wasm
echo ========================================
exit /b 0
