@echo off
rem PokeL-ARecomp for Windows ARM64 / Android: the game files and game code, made on a Windows PC (PC-Setup.ps1).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0PC-Setup.ps1" %*
echo.
pause
