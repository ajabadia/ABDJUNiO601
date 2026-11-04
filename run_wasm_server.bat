@echo off
rem ============================================================================
rem run_wasm_server.bat - Sirve la WebUI (con el modulo WASM ya compilado) en
rem http://localhost:8085 usando el servidor HTTP de Python.
rem Para recompilar el modulo antes, ejecuta wasm\build_wasm.bat.
rem ============================================================================

cd /d "%~dp0"

where python >nul 2>&1
if errorlevel 1 (
    echo [ERROR] python no esta en PATH.
    exit /b 1
)

echo ========================================================
echo   Iniciando Servidor Web WASM para ABD JUNiO 601
echo ========================================================
echo.
echo Servidor HTTP en ejecucion en http://localhost:8085
echo Abriendo la interfaz completa del sintetizador WebUI...
echo.

start "" http://localhost:8085/index.html
python -m http.server 8085 --directory Source\UI\WebUI
