@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_windows_prod.ps1" %*
exit /b %ERRORLEVEL%
