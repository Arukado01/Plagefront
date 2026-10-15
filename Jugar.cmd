@echo off
cd /d "%~dp0"
if not exist "build\Plaguefront.exe" (
  echo Primero compila el proyecto con Tools\BuildLocal.ps1
  pause
  exit /b 1
)
start "Plaguefront" "build\Plaguefront.exe"
