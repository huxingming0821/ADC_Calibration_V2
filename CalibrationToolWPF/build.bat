@echo off
echo ========================================
echo   ADC Calibration Tool Build Script
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

echo [INFO] .NET SDK found:
dotnet --version
echo.

echo [STEP 1/3] Restoring NuGet packages...
dotnet restore
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to restore packages!
    pause
    exit /b 1
)
echo.

echo [STEP 2/3] Building project (Release)...
dotnet build -c Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed!
    pause
    exit /b 1
)
echo.

echo [STEP 3/3] Build completed successfully!
echo.
echo ========================================
echo   EXE Location:
echo   bin\Release\net8.0-windows\CalibrationTool.exe
echo ========================================
echo.

REM Open output folder
explorer bin\Release\net8.0-windows\

pause
