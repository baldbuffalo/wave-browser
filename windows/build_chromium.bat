@echo off
setlocal enabledelayedexpansion

rem Wave Windows Chromium build.
rem Chromium is fetched at build time and is never committed to Wave.

if "%CHROMIUM_SRC%"=="" set "CHROMIUM_SRC=%CD%\..\..\chromium"
if "%DEPOT_TOOLS%"=="" set "DEPOT_TOOLS=%CD%\..\..\depot_tools"
if "%WAVE_OUT%"=="" set "WAVE_OUT=%CD%\out"

if not exist "%DEPOT_TOOLS%\gclient.bat" (
  echo depot_tools was not found. Set DEPOT_TOOLS to the depot_tools directory.
  exit /b 1
)

set "PATH=%DEPOT_TOOLS%;%PATH%"

if not exist "%CHROMIUM_SRC%\.gclient" (
  echo Fetching Chromium source...
  cd /d "%CHROMIUM_SRC%\.."
  call fetch.bat chromium
  if errorlevel 1 exit /b 1
)

cd /d "%CHROMIUM_SRC%"
call gclient sync --nohooks
if errorlevel 1 exit /b 1
call gclient runhooks
if errorlevel 1 exit /b 1

if not exist out\WaveWin mkdir out\WaveWin

gn gen out\WaveWin --args="is_debug=false is_component_build=false target_cpu=\"x64\""
if errorlevel 1 exit /b 1

autoninja -C out\WaveWin chrome
if errorlevel 1 exit /b 1

if not exist "%WAVE_OUT%" mkdir "%WAVE_OUT%"
copy /Y out\WaveWin\chrome.exe "%WAVE_OUT%\WaveBrowser.exe" >nul
if errorlevel 1 exit /b 1

echo.
echo WaveBrowser.exe created at:
echo %WAVE_OUT%\WaveBrowser.exe
