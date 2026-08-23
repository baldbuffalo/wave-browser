@echo off
setlocal enabledelayedexpansion

rem Wave Windows Chromium build bootstrap.
rem Chromium source is fetched into the build workspace, not committed to Wave.

if "%CHROMIUM_SRC%"=="" set "CHROMIUM_SRC=%CD%\..\..\chromium"
if "%DEPOT_TOOLS%"=="" set "DEPOT_TOOLS=%CD%\..\..\depot_tools"

if not exist "%DEPOT_TOOLS%\gclient.bat" (
  echo depot_tools was not found. Install depot_tools and set DEPOT_TOOLS.
  exit /b 1
)

set "PATH=%DEPOT_TOOLS%;%PATH%"

if not exist "%CHROMIUM_SRC%\.gclient" (
  echo Fetching Chromium source...
  call fetch.bat chromium
  if errorlevel 1 exit /b 1
)

cd /d "%CHROMIUM_SRC%"
call gclient sync --nohooks
if errorlevel 1 exit /b 1

call gclient runhooks
if errorlevel 1 exit /b 1

if not exist out\WaveWin (
  gn gen out\WaveWin --args="is_debug=false is_component_build=false target_cpu=\"x64\""
  if errorlevel 1 exit /b 1
)

autoninja -C out\WaveWin chrome
if errorlevel 1 exit /b 1

echo Wave Windows Chromium build completed.
