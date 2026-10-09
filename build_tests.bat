@echo off
setlocal enabledelayedexpansion

rem --- Rutas canónicas de vcvars para varias ediciones/versiones de Visual Studio ---
set "VC_VARS="
for %%A in (
    "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files\Microsoft Visual Studio\17\Community\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files\Microsoft Visual Studio\17\Professional\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files\Microsoft Visual Studio\17\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\17\Community\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\17\Professional\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\17\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
) do (
    if not defined VC_VARS if exist %%A set "VC_VARS=%%~A"
)

if not defined VC_VARS (
    echo [ERROR] vcvarsall.bat no encontrado.
    exit /b 1
)

set "CMAKE_PATH="
where cmake >nul 2>&1
if %ERRORLEVEL%==0 (
    for /f "delims=" %%F in ('where cmake') do (
        if not defined CMAKE_PATH set "CMAKE_PATH=%%F"
    )
)
if not defined CMAKE_PATH (
    set "CMAKE_PATH=cmake"
)

echo === Building JunoUnitTests (Super Six model, JUNO_TARGET_MODEL=0) ===
call "%VC_VARS%" x64

rem Limpieza completa si se solicita --clean o si se detecta configuracion obsoleta de VS
if "%~1"=="--clean" (
    if exist "build_supersix" (
        echo [INFO] Flag --clean detectado. Eliminando build_supersix...
        rmdir /s /q "build_supersix"
    )
)

if exist "build_supersix" (
    set "NEED_CLEAN="
    if exist "build_supersix\CMakeCache.txt" (
        findstr /C:"C:/Program Files/Microsoft Visual Studio/18/Community" "build_supersix\CMakeCache.txt" >nul 2>&1
        if !ERRORLEVEL!==0 set "NEED_CLEAN=1"
    )
    if exist "build_supersix\_juce\tools\CMakeCache.txt" (
        findstr /C:"C:/Program Files/Microsoft Visual Studio/18/Community" "build_supersix\_juce\tools\CMakeCache.txt" >nul 2>&1
        if !ERRORLEVEL!==0 set "NEED_CLEAN=1"
    )
    if exist "build_supersix\_juce\tools\ZERO_CHECK.vcxproj" (
        findstr /C:"Community" "build_supersix\_juce\tools\ZERO_CHECK.vcxproj" >nul 2>&1
        if !ERRORLEVEL!==0 set "NEED_CLEAN=1"
    )
    if defined NEED_CLEAN (
        echo [INFO] Detectada instancia obsoleta de VS en build_supersix. Eliminando directorio completo...
        rmdir /s /q "build_supersix"
    )
)

echo [INFO] Configurando CMake para build_supersix...
"%CMAKE_PATH%" -S . -B build_supersix -D JUNO_TARGET_MODEL=0
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed!
    exit /b 1
)

"%CMAKE_PATH%" --build build_supersix --config Release --target JunoUnitTests
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    exit /b 1
)
echo [SUCCESS] Build OK.
echo.

echo === Running JunoUnitTests ===
if exist "build_supersix\JunoUnitTests_artefacts\Release\JunoUnitTests.exe" (
    build_supersix\JunoUnitTests_artefacts\Release\JunoUnitTests.exe
) else if exist "build_supersix\Release\JunoUnitTests.exe" (
    build_supersix\Release\JunoUnitTests.exe
) else (
    echo Searching for JunoUnitTests.exe...
    dir /s /b build_supersix\JunoUnitTests.exe 2>nul
)

echo.
echo === Done ===
