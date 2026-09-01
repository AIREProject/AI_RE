@echo off
setlocal
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0start-presentation.ps1"
if errorlevel 1 (
  echo.
  echo Presentation startup failed.
  pause
)
endlocal
