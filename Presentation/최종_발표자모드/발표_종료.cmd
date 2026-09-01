@echo off
setlocal
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0stop-presentation.ps1"
if "%AIRE_PRESENTATION_VALIDATE_ONLY%"=="1" goto :done
echo.
pause
:done
endlocal
