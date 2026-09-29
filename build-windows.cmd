@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-windows.ps1" %*
set "result=%errorlevel%"
if not "%result%"=="0" echo Echec de la compilation. Voir le message ci-dessus.
if not defined CI pause
exit /b %result%
