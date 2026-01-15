@echo off
setlocal enabledelayedexpansion

echo ========================================
echo JUNiO 601 Build Script
echo ========================================
echo.

:: 1. Intentar localizar CMake automáticamente usando vswhere
set "CMAKE_PATH=cmake"
set "VSWHER_EXE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "%VSWHER_EXE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHER_EXE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath`) do (
        set "VS_PATH=%%i"
        set "POTENTIAL_CMAKE=!VS_PATH!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if exist "!POTENTIAL_CMAKE!" (
            set "CMAKE_PATH=!POTENTIAL_CMAKE!"
            echo [INFO] CMake localizado en: !CMAKE_PATH!
        )
    )
)

:: 2. Verificar si cmake es ejecutable
"%CMAKE_PATH%" --version >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] No se pudo encontrar CMake. 
    echo Por favor, asegúrate de que Visual Studio tenga instalado el componente 
    echo "Herramientas de CMake para C++" o que CMake esté en tu PATH.
    exit /b 1
)

:: 3. Incrementar versión de Build
set "VERSION_FILE=build_no.txt"
if not exist %VERSION_FILE% echo 0 > %VERSION_FILE%
set /p build_no=<%VERSION_FILE%
set /a build_no=%build_no% + 1
echo %build_no% > %VERSION_FILE%

echo #define JUNO_BUILD_VERSION "%build_no%" > "Source/Core/BuildVersion.h"
echo #define JUNO_BUILD_TIMESTAMP "%DATE% %TIME%" >> "Source/Core/BuildVersion.h"

:: 4. Configurar y Compilar
set BUILD_DIR=build
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

echo.
echo 1. Configurando proyecto...
"%CMAKE_PATH%" -B %BUILD_DIR% -A x64
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Error en la configuracion de CMake.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo 2. Compilando JUNiO 601 (Debug)...
"%CMAKE_PATH%" --build %BUILD_DIR% --config Debug --target ABDSimpleJuno106_Standalone
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] La compilacion ha fallado.
    exit /b %ERRORLEVEL%
)

:: 5. Mover ejecutable
echo.
echo 3. Finalizando...
set EXE_SRC=%BUILD_DIR%\ABDSimpleJuno106_artefacts\Debug\Standalone\ABDSimpleJuno106.exe
if not exist "%EXE_SRC%" set EXE_SRC=%BUILD_DIR%\Debug\ABDSimpleJuno106.exe

if exist "%EXE_SRC%" (
    copy /Y "%EXE_SRC%" "JUNiO_601.exe" >nul
    echo.
    echo ========================================
    echo BUILD EXITOSA! (Build #%build_no%)
    echo Ejecutable: JUNiO_601.exe
    echo ========================================
    echo [INFO] Build finalizado.

