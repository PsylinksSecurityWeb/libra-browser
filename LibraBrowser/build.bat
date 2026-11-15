@echo off
setlocal

REM One-click build for Libra Browser: restores the NuGet packages, then builds Release x64.
REM Run it from anywhere. Needs Visual Studio Build Tools (C++ workload, Windows SDK) and internet for NuGet.
REM The project uses toolset v145 (Visual Studio 2026). With Visual Studio 2022 run:  set TOOLSET=v143  and then build.bat

set "ROOT=%~dp0"
set "VCXPROJ=%ROOT%LibraBrowser.vcxproj"
set "OUT=%ROOT%x64\Release\LibraBrowser.exe"
set "NUGET=%ROOT%tools\nuget.exe"

REM 1. nuget.exe (downloaded once)
if not exist "%ROOT%tools" mkdir "%ROOT%tools"
if exist "%NUGET%" goto restore
echo [build] Downloading nuget.exe ...
powershell -NoProfile -Command "Invoke-WebRequest -Uri 'https://dist.nuget.org/win-x86-commandline/latest/nuget.exe' -OutFile '%NUGET%'"
if errorlevel 1 goto nonuget

:restore
echo [build] Restoring NuGet packages ...
"%NUGET%" restore "%VCXPROJ%" -PackagesDirectory "%ROOT%packages" -NonInteractive
if errorlevel 1 goto norestore

REM 2. MSBuild
set "MSBUILD="
set "VSW=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSW%" for /f "usebackq tokens=*" %%i in (`"%VSW%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
if not defined MSBUILD goto nomsbuild
echo [build] Using %MSBUILD%

REM 3. Build
set "TP="
if defined TOOLSET set "TP=/p:PlatformToolset=%TOOLSET%"
echo [build] Compiling Release x64 ...
"%MSBUILD%" "%VCXPROJ%" /p:Configuration=Release /p:Platform=x64 %TP% /m /nologo /v:minimal
if errorlevel 1 goto failed
if not exist "%OUT%" goto failed
echo.
echo Build succeeded: %OUT%
exit /b 0

:nonuget
echo [ERROR] Could not download nuget.exe. Check the internet connection.
exit /b 1
:norestore
echo [ERROR] NuGet restore failed.
exit /b 1
:nomsbuild
echo [ERROR] MSBuild not found. Install Visual Studio Build Tools with the "Desktop development with C++" workload.
exit /b 1
:failed
echo [ERROR] Build failed. See the messages above (error MSB8020 = toolset missing: see the note at the top of this file).
exit /b 1
