@echo off
setlocal
cd /d "%~dp0"

rem remember if it was started by double-click, so the window does not vanish
echo %cmdcmdline% | find /i "%~nx0" >nul && set "DBLCLICK=1"

rem --- compiler on PATH (finds Visual Studio and a complete SDK by itself) ---
call "%~dp0setenv.bat"
if errorlevel 1 goto fail

rem --- one-time: fetch the WebView2 SDK (headers + static loader) ---
if not exist wv2\build\native\include\WebView2.h (
  echo Downloading the WebView2 SDK, one time only...
  powershell -NoProfile -Command "Invoke-WebRequest https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2 -OutFile wv2.zip; Expand-Archive wv2.zip wv2 -Force; Remove-Item wv2.zip"
)
if not exist wv2\build\native\include\WebView2.h (
  echo Could not get the WebView2 SDK. Check your internet connection.
  goto fail
)

rem --- version comes from version.txt (single source of truth) ---
set "WKVER=2.0.0"
if exist version.txt set /p WKVER=<version.txt

rem --- embed index.html, the manifest and the icon into a resource file ---
rc /nologo app.rc || goto fail

rem --- compile and link a single static exe ---
cl /nologo /O2 /EHsc /MT /std:c++17 /utf-8 /W3 /DWINKIT_VERSION=L\"%WKVER%\" winkit.cpp app.res /I wv2\build\native\include /link /SUBSYSTEM:WINDOWS /OUT:WinKit.exe wv2\build\native\x64\WebView2LoaderStatic.lib user32.lib gdi32.lib dwmapi.lib shlwapi.lib || goto fail

echo.
echo Built WinKit.exe %WKVER%

rem   build.bat run   -> build, then start it
rem   build.bat dev   -> build, then start it loading .\index.html from disk (F12 = devtools)
if /i "%~1"=="run" start "" WinKit.exe
if /i "%~1"=="dev" start "" WinKit.exe --dev

if defined DBLCLICK pause
exit /b 0

:fail
echo.
echo Build failed.
if defined DBLCLICK pause
exit /b 1
