@echo off
REM Builds LibraBrowser-Setup-3.9.0.exe (embeds the release exe). Run build of the browser first.
setlocal
set "H=%~dp0"
copy /y "%H%..\LibraBrowser\x64\Release\LibraBrowser.exe" "%H%LibraBrowser.exe" >nul
copy /y "%H%..\LibraBrowser\src\libra.ico" "%H%libra.ico" >nul
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%H%"
rc /nologo installer.rc || exit /b 1
copy /y "%H%..\LibraBrowser\src\SafetyAuth.cpp" "%H%SafetyAuth.cpp" >nul
cl /nologo /std:c++20 /O1 /MT /GS /guard:cf /EHsc /DUNICODE /D_UNICODE installer.cpp SafetyAuth.cpp installer.res /Fe:LibraBrowser-Setup-3.9.0.exe /link windowsapp.lib /SUBSYSTEM:WINDOWS /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /CETCOMPAT || exit /b 1
del installer.obj >nul 2>&1
echo built
