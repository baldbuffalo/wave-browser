@echo off
setlocal enabledelayedexpansion

rem Wave Windows Chromium build.
rem Chromium is fetched at build time and is never committed to Wave.

if "%CHROMIUM_SRC%"=="" set "CHROMIUM_SRC=%CD%\..\..\chromium"
if "%DEPOT_TOOLS%"=="" set "DEPOT_TOOLS=%CD%\..\..\depot_tools"
if "%WAVE_OUT%"=="" set "WAVE_OUT=%CD%\out"

if defined DEPOT_TOOLS if exist "%DEPOT_TOOLS%\gclient.bat" goto depot_tools_ready
if exist "%RUNNER_TEMP%\depot_tools\gclient.bat" set "DEPOT_TOOLS=%RUNNER_TEMP%\depot_tools"
if exist "%DEPOT_TOOLS%\gclient.bat" goto depot_tools_ready

echo depot_tools was not found. Set DEPOT_TOOLS to the depot_tools directory.
exit /b 1

:depot_tools_ready
set "PATH=%DEPOT_TOOLS%;%PATH%"

if /I "%1"=="--fetch-only" goto fetch_only
if /I "%1"=="--build-only" goto build_only

goto sync_and_build

:fetch_only
if not exist "%CHROMIUM_SRC%\.gclient" (
  echo Fetching Chromium source...
  cd /d "%CHROMIUM_SRC%\.."
  call "%DEPOT_TOOLS%\fetch.bat" chromium
  if errorlevel 1 exit /b 1
) else (
  echo Chromium source already exists.
)
exit /b 0

:sync_and_build
if not exist "%CHROMIUM_SRC%\.gclient" (
  echo Fetching Chromium source...
  cd /d "%CHROMIUM_SRC%\.."
  call "%DEPOT_TOOLS%\fetch.bat" chromium
  if errorlevel 1 exit /b 1
)

goto continue_build

:build_only
if not exist "%CHROMIUM_SRC%\.gclient" (
  echo Chromium source cache was not restored.
  exit /b 1
)

goto continue_build

:continue_build
cd /d "%CHROMIUM_SRC%"
call "%DEPOT_TOOLS%\gclient.bat" sync --nohooks
if errorlevel 1 exit /b 1
call "%DEPOT_TOOLS%\gclient.bat" runhooks
if errorlevel 1 exit /b 1

python "%~dp0apply_wave_chromium_patch.py"
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
