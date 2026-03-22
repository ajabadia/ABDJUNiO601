@echo off
set "VC_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
if not exist "%VC_PATH%" set "VC_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional"
if not exist "%VC_PATH%" set "VC_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise"

set "CMAKE_PATH=%VC_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if not exist "%CMAKE_PATH%" (
    echo [ERROR] CMake not found at %CMAKE_PATH%
    exit /b 1
)

echo [INFO] Using CMake: "%CMAKE_PATH%"
"%CMAKE_PATH%" -B build -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Config failed with %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

"%CMAKE_PATH%" --build build --config Debug --target ABDSimpleJuno106_Standalone
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed with %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)
echo [SUCCESS] Build completed.
