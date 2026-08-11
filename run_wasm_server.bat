@echo off
echo ========================================================
echo   Iniciando Servidor Web WASM para ABD JUNiO 601
echo ========================================================
echo.
echo Servidor HTTP en ejecucion en http://localhost:8085
echo Abriendo la interfaz completa del sintetizador WebUI...
echo.

start http://localhost:8085/index.html
python -m http.server 8085 --directory Source/UI/WebUI

pause


