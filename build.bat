@echo off
gcc main.c -o LibraryManagementSystem.exe -mwindows -lcomdlg32
if errorlevel 1 (
  echo.
  echo Build failed. Check that GCC is installed and available in PATH.
  pause
  exit /b 1
)
echo Build successful: LibraryManagementSystem.exe
pause
