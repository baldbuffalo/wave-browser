@echo off
setlocal enabledelayedexpansion

rem Wave Windows Chromium build.
rem Chromium is fetched at build time and is never committed to Wave.

if "%CHROMIUM_SRC%"=="" set "CHROMIUM_SRC=%CD%\..\..\chromium"
if "%DEPOT_TOOLS%"=="" set "DEPOT_TOOLS=%CD%\..\..\depot_tools"
if "%WAVE_OUT%"=="" set "WAVE_OUT=%CD%\out"

rem On GitHub Actions, depot_tools is installed by update-chromium.yml.
rem Always verify the environment variable first, then fall back to a local checkout.
if defined DEPOT_TOOLS if exist "%DEPOT_TOOLS%\gclient.bat" goto depot_tools_ready
if exist "%RUNNER_TEMP%\depot_tools\gclient.bat" set "DEPOT_TOOLS=%RUNNER_TEMP%\depot_tools"
if exist "%DEPOT_TOOLS%\gclient.bat" goto depot_tools_ready

echo depot_tools was not found. Set DEPOT_TOOLS to the depot_tools directory.
exit /b 1

:depot_tools_ready
set "PATH=%DEPOT_TOOLS%;%PATH%"

if not exist "%CHROMIUM_SRC%\.gclient" (
  echo Fetching Chromium source...
  cd /d "%CHROMIUM_SRC%\.."
  call "%DEPOT_TOOLS%\fetch.bat" chromium
  if errorlevel 1 exit /b 1
)

cd /d "%CHROMIUM_SRC%"
call "%DEPOT_TOOLS%\gclient.bat" sync --nohooks
if errorlevel 1 exit /b 1
call "%DEPOT_TOOLS%\gclient.bat" runhooks
if errorlevel 1 exit /b 1

rem Apply Wave's desktop UI directly to the fetched Chromium source.
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
