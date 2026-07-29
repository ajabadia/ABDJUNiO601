@echo off
REM ============================================================
REM build_wasm.bat — Compila el motor DSP de ABDJUNiO601 a WASM
REM ============================================================

echo ========================================
echo  ABDJUNiO601 WASM Build
echo ========================================

REM 1. Limpieza total de shims
if exist "wasm\juce_core" (
    echo Cleaning juce_core shim...
    rmdir /s /q "wasm\juce_core"
)
if exist "wasm\juce_audio_basics" (
    echo Cleaning juce_audio_basics shim...
    rmdir /s /q "wasm\juce_audio_basics"
)

REM 2. Copiar juce_core localmente
echo Copying juce_core module...
powershell -NoProfile -Command "Copy-Item -Path 'C:\JUCE\modules\juce_core' -Destination 'wasm\juce_core' -Recurse -Force"

REM 3. Patching ThreadPriorities de juce_core
echo Overriding ThreadPriorities for Emscripten...
copy /y "wasm\juce_shim\native\juce_ThreadPriorities_native.h" "wasm\juce_core\native\juce_ThreadPriorities_native.h" >nul

REM 4. Copiar juce_dsp localmente
if exist "wasm\juce_dsp" (
    echo Cleaning juce_dsp shim...
    rmdir /s /q "wasm\juce_dsp"
)
echo Copying juce_dsp module...
powershell -NoProfile -Command "Copy-Item -Path 'C:\JUCE\modules\juce_dsp' -Destination 'wasm\juce_dsp' -Recurse -Force"

REM 5. Copiar juce_audio_basics localmente
echo Copying juce_audio_basics module...
powershell -NoProfile -Command "Copy-Item -Path 'C:\JUCE\modules\juce_audio_basics' -Destination 'wasm\juce_audio_basics' -Recurse -Force"

REM 6. Añadir CMake al PATH (no está en PATH por defecto)
set "PATH=C:\Program Files\CMake\bin;%PATH%"

REM 7. Activar Emscripten
call C:\emsdk\emsdk_env.bat >nul 2>nul

REM 8. Crear directorio de build
if not exist "wasm\build" mkdir wasm\build

REM 9. Configurar CMake
echo Configuring CMake with Emscripten...
call emcmake cmake -S wasm -B wasm\build -DCMAKE_BUILD_TYPE=Release -G Ninja
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed!
    exit /b 1
)

REM 10. Compilar
echo Building WASM module...
cmake --build wasm\build
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed!
    exit /b 1
)

echo.
echo ========================================
echo  Build complete!
echo  Output: WebUI\wasm\abdjunio601_wasm.js
echo          WebUI\wasm\abdjunio601_wasm.wasm
echo ========================================
