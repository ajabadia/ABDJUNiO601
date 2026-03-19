@echo off
setlocal enabledelayedexpansion
echo ========================================
echo JUNiO 601 Automated Build
echo ========================================

set "CMAKE_PATH=cmake"
set "BUILD_DIR=build"
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

echo 1. Configurando proyecto...
"%CMAKE_PATH%" -B %BUILD_DIR% -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DBUILD_SHARED_LIBS=OFF
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

echo 2. Compilando JUNiO 601 (Release)...
"%CMAKE_PATH%" --build %BUILD_DIR% --config Release --target ABDSimpleJuno106_Standalone
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

echo 3. Finalizando...
echo BUILD EXITOSA!
exit /b 0
