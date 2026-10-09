@echo off
setlocal EnableDelayedExpansion

if "%~1"=="" (
    echo Drag and drop your C++ file onto this script, or run: build.bat plugin.cpp
    pause
    exit /b 1
)

where cl >nul 2>&1
if errorlevel 1 (
    echo Searching for Visual Studio environment...

    set "PF86=%ProgramFiles(x86)%"
    set "VSWHERE=!PF86!\Microsoft Visual Studio\Installer\vswhere.exe"
    set "VS_PATH="

    if exist "!VSWHERE!" (
        for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%i"

        if not defined VS_PATH (
            for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -products * -property installationPath`) do set "VS_PATH=%%i"
        )
    ) else (
        echo vswhere.exe not found at: !VSWHERE!
    )

    if defined VS_PATH (
        echo Found Visual Studio at: !VS_PATH!
        if exist "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat" (
            call "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat"
        ) else if exist "!VS_PATH!\Common7\Tools\VsDevCmd.bat" (
            call "!VS_PATH!\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
        )
    )
)

where cl >nul 2>&1
if errorlevel 1 (
    echo.
    echo [Error] Could not find the Visual Studio C++ compiler ^(cl.exe^).
    echo Open the Visual Studio Installer, click Modify, and make sure
    echo "Desktop development with C++" is ticked.
    pause
    exit /b 1
)

echo Compiling %~nx1...
cl /nologo /std:c++17 /EHsc /MD /LD /I"%~dp0shared" /Fo"%TEMP%\plugin_%RANDOM%.obj" "%~f1" /link /NOIMPLIB /NOEXP /OUT:"%~dpn1.dll"
set "RESULT=%ERRORLEVEL%"

echo.
echo ========================================
if "%RESULT%"=="0" (
    echo Build successful! DLL created at:
    echo %~dpn1.dll
) else (
    echo Build failed with code %RESULT%.
)
echo ========================================

pause