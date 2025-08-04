@echo off
:: Check if main.cpp exists
if not exist "main.cpp" (
    echo main.cpp not found.
    pause
    exit /b
)

:: Compile main.cpp to main.exe
g++ main.cpp -o main.exe
if errorlevel 1 (
    echo Compilation failed.
    pause
    exit /b
)

:: Run the executable
main.exe
