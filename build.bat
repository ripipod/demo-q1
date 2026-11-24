@echo off
setlocal
where astroemu-cli.exe >nul 2>nul
if errorlevel 1 (
  echo astroemu-cli.exe was not found in PATH.
  echo Run this from your AstroEMU folder or add AstroEMU to PATH.
  exit /b 1
)
astroemu-cli.exe --compile-folder "%~dp0src" --out "%~dp0build"
if errorlevel 1 exit /b %errorlevel%
echo.
echo Built: "%~dp0build\program.qsb"
endlocal
