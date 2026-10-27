@echo off
if not exist LibraryManagementSystem.exe (
  echo EXE not found. Run build.bat first.
  pause
  exit /b 1
)
start "" "%~dp0LibraryManagementSystem.exe"
