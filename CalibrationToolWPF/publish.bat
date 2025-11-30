@echo off
echo ========================================
echo   ADC Calibration Tool Publish Script
echo   WPF Version with Material Design
echo ========================================
echo.

REM Check if dotnet is available
where dotnet >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] .NET SDK not found!
    echo Please install .NET 8.0 SDK from:
    echo https://dotnet.microsoft.com/download/dotnet/8.0
    pause
    exit /b 1
)

echo Select publish mode:
echo [1] Framework-dependent (smaller, requires .NET 8.0 runtime)
echo [2] Self-contained x64 (larger, runs without .NET runtime)
echo [3] Self-contained x86 (larger, runs without .NET runtime)
echo.
set /p choice=Enter choice (1/2/3): 

if "%choice%"=="1" (
    echo.
    echo [INFO] Publishing framework-dependent...
    dotnet publish -c Release -o publish\framework-dependent
    echo.
    echo Output: publish\framework-dependent\CalibrationTool.exe
    explorer publish\framework-dependent\
) else if "%choice%"=="2" (
    echo.
    echo [INFO] Publishing self-contained x64...
    dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o publish\win-x64
    echo.
    echo Output: publish\win-x64\CalibrationTool.exe
    explorer publish\win-x64\
) else if "%choice%"=="3" (
    echo.
    echo [INFO] Publishing self-contained x86...
    dotnet publish -c Release -r win-x86 --self-contained true -p:PublishSingleFile=true -o publish\win-x86
    echo.
    echo Output: publish\win-x86\CalibrationTool.exe
    explorer publish\win-x86\
) else (
    echo [ERROR] Invalid choice!
    pause
    exit /b 1
)

echo.
echo [SUCCESS] Publish completed!
echo.
pause
