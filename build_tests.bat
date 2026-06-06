@echo off
setlocal enabledelayedexpansion

set VS_PATH=C:\Program Files\Microsoft Visual Studio\18\Community
set CMAKE_PATH=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe

echo === Building JunoUnitTests (Super Six model, JUNO_TARGET_MODEL=0) ===
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64
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
