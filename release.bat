@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"
echo %cmdcmdline% | find /i "%~nx0" >nul && set "DBLCLICK=1"

rem Builds WinKit.exe, then produces everything for a release in .\release :
rem   WinKit-<v>-portable.exe   the single portable program (just run it, nothing to install)
rem   WinKit-<v>-portable.zip   same exe zipped (this is the asset name WinKit's self-updater looks for)
rem   WinKitSetup-<v>.exe       the Inno Setup installer (needs Inno Setup 6 on THIS machine only)
rem   SHA256SUMS.txt
rem Change the version by editing version.txt (and the two version numbers in app.rc).

set "WKVER=2.0.0"
if exist version.txt set /p WKVER=<version.txt

rem build.bat must not pause when called from here
set "DBLCLICK="
call "%~dp0build.bat"
if errorlevel 1 goto fail

if exist release rmdir /s /q release
mkdir release

copy /y WinKit.exe "release\WinKit-%WKVER%-portable.exe" >nul || goto fail
powershell -NoProfile -Command "Compress-Archive -Path 'WinKit.exe' -DestinationPath 'release\WinKit-%WKVER%-portable.zip' -CompressionLevel Optimal" || goto fail

rem --- find Inno Setup's compiler ---
set "ISCC="
where ISCC.exe >nul 2>nul && for /f "delims=" %%i in ('where ISCC.exe') do if not defined ISCC set "ISCC=%%i"
if not defined ISCC if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not defined ISCC if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not defined ISCC if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"

if defined ISCC (
  echo Building the installer...
  "%ISCC%" /Qp /DMyAppVersion=%WKVER% installer.iss || goto fail
) else (
  echo.
  echo Inno Setup 6 was not found, so the installer was skipped.
  echo Install it once with:   winget install JRSoftware.InnoSetup
  echo then run release.bat again.
)

rem --- checksums ---
powershell -NoProfile -Command "Get-ChildItem release -File | ForEach-Object { '{0}  {1}' -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower(), $_.Name } | Set-Content -Encoding ascii release\SHA256SUMS.txt"

echo.
echo Release %WKVER% is in the "release" folder:
dir /b release
if defined DBLCLICK pause
exit /b 0

:fail
echo.
echo Release failed.
if defined DBLCLICK pause
exit /b 1
