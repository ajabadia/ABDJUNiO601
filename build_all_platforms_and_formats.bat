@echo off
setlocal enabledelayedexpansion

echo =======================================================================
2: JUNiO 601 - BUILD ALL MODELS AND ALL FORMATS (Windows)
echo =======================================================================

set "VC_VARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
set "CMAKE_PATH=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if exist "%VC_VARS%" (
    call "%VC_VARS%" x64
) else (
    echo [WARNING] vcvarsall.bat not found at %VC_VARS%
)

if not exist "%CMAKE_PATH%" (
    echo [ERROR] CMake not found at %CMAKE_PATH%
    exit /b 1
)

rem --- Increment build number ---
set "VERSION_FILE=build_no.txt"
if not exist %VERSION_FILE% echo 0 > %VERSION_FILE%
set /p build_no=<%VERSION_FILE%
set /a build_no=%build_no% + 1
echo %build_no% > %VERSION_FILE%

echo #define JUNO_BUILD_VERSION "%build_no%" > "Source/Core/BuildVersion.h"
echo #define JUNO_BUILD_TIMESTAMP "%DATE% %TIME%" >> "Source/Core/BuildVersion.h"

set EXIT_CODE=0
for %%M in (0 1 2 3) do (
    call :BuildModel %%M
)

if !EXIT_CODE! NEQ 0 (
    echo.
    echo =======================================================================
    echo [ERROR] SOME MODELS OR FORMATS FAILED TO BUILD - Check output above
    echo =======================================================================
    exit /b !EXIT_CODE!
)

echo.
echo =======================================================================
echo [SUCCESS] ALL MODELS AND FORMATS (Standalone + VST3) BUILT SUCCESSFULLY
echo =======================================================================
exit /b 0

:BuildModel
setlocal
set MODEL=%1
if %MODEL%==0 set "NAME=Super Six" & set DIR=build_supersix
if %MODEL%==1 set "NAME=ABD JUNiO 601 (J-106)" & set DIR=build_j106
if %MODEL%==2 set "NAME=ABD JUNiO 06 (J-60)" & set DIR=build_j60
if %MODEL%==3 set "NAME=ABD JUNiO SIX (J-6)" & set DIR=build_j6

echo.
echo =======================================================================
echo Building all formats for: !NAME! (JUNO_TARGET_MODEL=%MODEL%)
echo =======================================================================

if not exist %DIR% mkdir %DIR%

"%CMAKE_PATH%" -S . -B %DIR% -G "Visual Studio 18 2026" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.26100.0 -DJUNO_TARGET_MODEL=%MODEL%
if !ERRORLEVEL! NEQ 0 (
    echo [ERROR] CMake configuration failed for !NAME!
    endlocal & set EXIT_CODE=1
    goto :eof
)

echo [INFO] Compiling Standalone, VST3 and other configured formats...
"%CMAKE_PATH%" --build %DIR% --config Release
if !ERRORLEVEL! NEQ 0 (
    echo [ERROR] Build failed for !NAME!
    endlocal & set EXIT_CODE=1
    goto :eof
)

echo [SUCCESS] !NAME! built successfully.
endlocal & set EXIT_CODE=%EXIT_CODE%
goto :eof
