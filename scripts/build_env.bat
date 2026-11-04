@echo off
rem ============================================================================
rem build_env.bat - Deteccion unica de toolchain para ABDJUNiO601
rem ----------------------------------------------------------------------------
rem Detecta Visual Studio (BuildTools o Community/Professional/Enterprise, VS 17
rem o 18), CMake y el Emscripten SDK, y deja las variables listas para usar:
rem
rem   VS_PATH       instancia de Visual Studio encontrada
rem   VC_VARS       ...\VC\Auxiliary\Build\vcvarsall.bat
rem   VS_GENERATOR  generador de CMake acorde a la version instalada
rem   VS_INSTANCE   valor para -DCMAKE_GENERATOR_INSTANCE
rem   CMAKE_PATH    ejecutable de CMake (PATH o el que trae Visual Studio)
rem   EMSDK_PATH    raiz del emsdk (si existe)
rem   EMSDK_ENV     ...\emsdk_env.bat
rem   JUCE_PATH     raiz de JUCE (con CMakeLists.txt y modules\)
rem
rem Uso:  call "%~dp0scripts\build_env.bat"
rem       -> errorlevel 1 si no hay Visual Studio o CMake.
rem No usa setlocal a proposito: las variables deben sobrevivir a la llamada.
rem ============================================================================

rem ProgramFiles(x86) se expande a variables simples: el nombre con parentesis
rem rompe el parseo de cualquier bloque if/for en el que se use directamente.
set "PF=%ProgramFiles%"
set "PF86=%ProgramFiles(x86)%"

rem --- Visual Studio ----------------------------------------------------------
set "VSWHERE=%PF86%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_PATH="
if exist "%VSWHERE%" call :VsWhere
if not defined VS_PATH call :ProbeVsPaths
if not defined VS_PATH (
    echo [ERROR] Visual Studio con herramientas C++ ^(MSVC^) no encontrado.
    exit /b 1
)
set "VC_VARS=%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat"

rem El generador tiene que coincidir con la version instalada; si no, CMake
rem aborta con "could not find specified instance of Visual Studio".
set "VS_GENERATOR=Visual Studio 18 2026"
set "VS_PROBE=%VS_PATH:\18\=%"
if "%VS_PROBE%"=="%VS_PATH%" set "VS_GENERATOR=Visual Studio 17 2022"
set "VS_INSTANCE=%VS_PATH%"

rem --- CMake ------------------------------------------------------------------
set "CMAKE_PATH="
for /f "delims=" %%C in ('where cmake 2^>nul') do if not defined CMAKE_PATH set "CMAKE_PATH=%%C"
if not defined CMAKE_PATH if exist "%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "CMAKE_PATH=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not defined CMAKE_PATH (
    echo [ERROR] CMake no encontrado ^(ni en PATH ni en Visual Studio^).
    exit /b 1
)

rem --- Emscripten -------------------------------------------------------------
rem Orden: EMSDK del entorno, D:\desarrollos\emsdk, C:\emsdk.
set "EMSDK_PATH="
if defined EMSDK if exist "%EMSDK%\emsdk_env.bat" set "EMSDK_PATH=%EMSDK%"
if not defined EMSDK_PATH if exist "D:\desarrollos\emsdk\emsdk_env.bat" set "EMSDK_PATH=D:\desarrollos\emsdk"
if not defined EMSDK_PATH if exist "C:\emsdk\emsdk_env.bat" set "EMSDK_PATH=C:\emsdk"
set "EMSDK_ENV="
if defined EMSDK_PATH set "EMSDK_ENV=%EMSDK_PATH%\emsdk_env.bat"

rem --- JUCE -------------------------------------------------------------------
rem Orden: JUCE_PATH del entorno, D:\desarrollos\juce (copia de referencia),
rem la copia del repo y, por ultimo, C:\JUCE. Los .bat lo pasan a CMake como
rem -DJUCE_PATH para que no se use en silencio una copia distinta del repo.
set "JUCE_REPO="
if exist "%~dp0..\JUCE\CMakeLists.txt" for %%I in ("%~dp0..\JUCE") do set "JUCE_REPO=%%~fI"
if not defined JUCE_PATH if exist "D:\desarrollos\juce\CMakeLists.txt" set "JUCE_PATH=D:\desarrollos\juce"
if not defined JUCE_PATH if defined JUCE_REPO set "JUCE_PATH=%JUCE_REPO%"
if not defined JUCE_PATH if exist "C:\JUCE\CMakeLists.txt" set "JUCE_PATH=C:\JUCE"

exit /b 0

rem ============================================================================
rem Subrutinas
rem ============================================================================

:VsWhere
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    if not defined VS_PATH set "VS_PATH=%%i"
)
goto :eof

:ProbeVsPaths
for %%V in (
    "%PF%\Microsoft Visual Studio\18\Community"
    "%PF%\Microsoft Visual Studio\18\Professional"
    "%PF%\Microsoft Visual Studio\18\Enterprise"
    "%PF86%\Microsoft Visual Studio\18\BuildTools"
    "%PF%\Microsoft Visual Studio\17\Community"
    "%PF%\Microsoft Visual Studio\17\Professional"
    "%PF86%\Microsoft Visual Studio\17\BuildTools"
) do (
    if not defined VS_PATH if exist "%%~V\VC\Auxiliary\Build\vcvarsall.bat" set "VS_PATH=%%~V"
)
goto :eof
