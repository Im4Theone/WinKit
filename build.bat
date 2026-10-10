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
set "WKVER=2.1.0"
if exist version.txt set /p WKVER=<version.txt

rem --- pack index.html (Windows LZMS compression, about 5x smaller) and embed it with the manifest and the icon ---
cl /nologo /O1 /EHsc /MT /W3 /D_CRT_SECURE_NO_WARNINGS packui.cpp /Fo"%TEMP%\packui.obj" /Fe:packui.exe /link /SUBSYSTEM:CONSOLE cabinet.lib >nul || goto fail
"%~dp0packui.exe" index.html index.pack || goto fail
del packui.exe >nul 2>nul
rc /nologo app.rc || goto fail

rem --- compile and link a single static exe (optimised for SIZE: /O1 + link-time code generation; WinKit is never CPU-bound) ---
cl /nologo /O1 /GL /Gy /Gw /EHsc /MT /std:c++17 /utf-8 /W3 /DWINKIT_VERSION=L\"%WKVER%\" winkit.cpp app.res /I wv2\build\native\include /link /SUBSYSTEM:WINDOWS /LTCG /OPT:REF /OPT:ICF /OUT:WinKit.exe wv2\build\native\x64\WebView2LoaderStatic.lib user32.lib gdi32.lib dwmapi.lib shlwapi.lib || goto fail

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
