@echo off
rem setenv.bat - puts cl, rc and link on PATH in the CURRENT cmd window.
rem No special "Native Tools" prompt needed. It finds Visual Studio / Build Tools itself
rem and picks the newest Windows SDK that is actually complete (has the normal headers).
rem
rem   Use it directly:   call setenv.bat     (then cl / rc / link work in this window)
rem   build.bat calls it automatically.

where cl >nul 2>nul
if not errorlevel 1 goto :eof

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto no_vs

set "VSROOT="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT goto no_vs

set "KITS=%ProgramFiles(x86)%\Windows Kits\10"
set "SDKVER="
for /f "delims=" %%v in ('dir /b /ad /o:n "%KITS%\Include" 2^>nul') do (
  if exist "%KITS%\Include\%%v\um\windows.h" if exist "%KITS%\Include\%%v\ucrt\ctype.h" set "SDKVER=%%v"
)
if not defined SDKVER goto no_sdk

rem Visual Studio's own scripts start helper tools by bare name; that fails (noisy "vswhere" error) when the current
rem folder is excluded from the search path, so lift that restriction just for this call.
set "WK_NDCD=%NoDefaultCurrentDirectoryInExePath%"
set "NoDefaultCurrentDirectoryInExePath="
call "%VSROOT%\VC\Auxiliary\Build\vcvarsall.bat" x64 %SDKVER% >nul
if defined WK_NDCD set "NoDefaultCurrentDirectoryInExePath=%WK_NDCD%"
set "WK_NDCD="
where cl >nul 2>nul
if errorlevel 1 goto no_vs
goto :eof

:no_vs
echo Could not find the C++ build tools.
echo Install "Build Tools for Visual Studio" with the "Desktop development with C++" workload.
exit /b 1

:no_sdk
echo Found Visual Studio, but no complete Windows SDK.
echo In the Visual Studio Installer, add a "Windows 11 SDK" under Individual components.
exit /b 1
