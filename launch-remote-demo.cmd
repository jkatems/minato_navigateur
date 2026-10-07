@echo off
setlocal
cd /d "%~dp0"
if exist "bin\minato.exe" (
    start "Minato Demonstration" "bin\minato.exe" --remote-demo --profile Demonstration
    exit /b
)
rem Prefer the deployed bundle, which already includes Qt DLLs and WebEngine resources.
if exist "dist\windows" (
    for /f "delims=" %%D in ('dir /b /ad /o-n "dist\windows"') do (
        if exist "dist\windows\%%D\Minato\bin\minato.exe" (
            start "Minato Demonstration" "dist\windows\%%D\Minato\bin\minato.exe" --remote-demo --profile Demonstration
            exit /b
        )
    )
)
if exist "build-windows\Release\minato.exe" (
    start "Minato Demonstration" "build-windows\Release\minato.exe" --remote-demo --profile Demonstration
    exit /b
)
if exist "build\Release\minato.exe" (
    start "Minato Demonstration" "build\Release\minato.exe" --remote-demo --profile Demonstration
    exit /b
)
if exist "build\minato.exe" (
    start "Minato Demonstration" "build\minato.exe" --remote-demo --profile Demonstration
    exit /b
)
echo Compilez Minato avec build-windows.cmd puis relancez ce fichier.
pause
exit /b 1
