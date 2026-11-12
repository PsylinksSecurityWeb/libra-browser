@echo off
setlocal

REM ─────────────────────────────────────────────────────────────────────────
REM  build.bat — One-click build for Libra Browser
REM  Restores NuGet packages then compiles with MSBuild (Release x64).
REM  Run this from the LibraBrowser\ directory.
REM ─────────────────────────────────────────────────────────────────────────

set "ROOT=%~dp0"
set "VCXPROJ=%ROOT%LibraBrowser.vcxproj"
set "PKGS=%ROOT%packages"
set "OUT=%ROOT%bin\x64\Release\LibraBrowser.exe"

REM ── 1. Find nuget.exe (download if missing) ──────────────────────────────
set "NUGET=%ROOT%tools\nuget.exe"
if not exist "%ROOT%tools" mkdir "%ROOT%tools"
if not exist "%NUGET%" (
    echo [build] Downloading nuget.exe...
    powershell -NoProfile -Command ^
      "Invoke-WebRequest -Uri 'https://dist.nuget.org/win-x86-commandline/latest/nuget.exe' -OutFile '%NUGET%'"
    if errorlevel 1 (
        echo [ERROR] Failed to download nuget.exe. Check internet connection.
        exit /b 1
    )
)

REM ── 2. Restore NuGet packages ─────────────────────────────────────────────
echo [build] Restoring NuGet packages...
"%NUGET%" restore "%VCXPROJ%" -PackagesDirectory "%PKGS%" -NonInteractive
if errorlevel 1 (
    echo [ERROR] NuGet restore failed.
    exit /b 1
)

REM ── 3. Find MSBuild ───────────────────────────────────────────────────────
set "MSBUILD="
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do set "MSBUILD=%%i"

if not defined MSBUILD (
    REM Fallback: try known path for Build Tools 2022
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    )
)
if not defined MSBUILD (
    echo [ERROR] MSBuild not found. Install Visual Studio Build Tools.
    exit /b 1
)
echo [build] Using MSBuild: %MSBUILD%

REM ── 4. Create placeholder icons if missing ───────────────────────────────
if not exist "%ROOT%src\libra.ico" (
    echo [build] No libra.ico found — using shell32 placeholder...
    powershell -NoProfile -Command ^
      "$shell32 = 'C:\Windows\System32\shell32.dll'; " ^
      "[System.IO.File]::Copy($shell32, '%ROOT%src\libra.ico', $true)" 2>nul
    REM Actually extract a real icon via PowerShell
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
      "Add-Type -AssemblyName System.Drawing;" ^
      "$icon = [System.Drawing.Icon]::ExtractAssociatedIcon('C:\Windows\System32\msedge.exe');" ^
      "if($icon){$icon.Save('%ROOT%src\libra.ico')}" 2>nul
    REM If still missing, copy any .ico
    if not exist "%ROOT%src\libra.ico" copy /y "%windir%\system32\shell32.dll" "%ROOT%src\libra.ico" >nul 2>&1
)
if not exist "%ROOT%src\incognito.ico" copy /y "%ROOT%src\libra.ico" "%ROOT%src\incognito.ico" >nul 2>&1

REM ── 5. Build ──────────────────────────────────────────────────────────────
echo [build] Compiling Release x64...
"%MSBUILD%" "%VCXPROJ%" /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal
if errorlevel 1 (
    echo.
    echo [ERROR] Build failed. See output above.
    exit /b 1
)

echo.
echo ============================================================
echo  Build succeeded!
echo  Output: %OUT%
if exist "%OUT%" (
    for %%F in ("%OUT%") do echo  Size:   %%~zF bytes
)
echo ============================================================
echo.
pause
