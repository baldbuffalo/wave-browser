@echo off
setlocal enabledelayedexpansion

rem Wave Windows Chromium build.
rem Chromium is supplied by update-chromium.yml through OneDrive.

if "%CHROMIUM_SRC%"=="" set "CHROMIUM_SRC=%CD%\..\..\chromium"
if "%DEPOT_TOOLS%"=="" set "DEPOT_TOOLS=%CD%\..\..\depot_tools"
if "%GIT_CACHE_PATH%"=="" set "GIT_CACHE_PATH=%CD%\..\..\git-cache"
if "%WAVE_OUT%"=="" set "WAVE_OUT=%CD%\out"

if defined DEPOT_TOOLS if exist "%DEPOT_TOOLS%\gclient.bat" goto depot_tools_ready
if exist "%RUNNER_TEMP%\depot_tools\gclient.bat" set "DEPOT_TOOLS=%RUNNER_TEMP%\depot_tools"
if defined DEPOT_TOOLS if exist "%DEPOT_TOOLS%\gclient.bat" goto depot_tools_ready

echo depot_tools was not found. Set DEPOT_TOOLS to the depot_tools directory.
exit /b 1

:depot_tools_ready
set "PATH=%DEPOT_TOOLS%;%PATH%"
set "GIT_CACHE_PATH=%GIT_CACHE_PATH%"

if not exist "%CHROMIUM_SRC%\.git" (
  echo Chromium checkout is missing. Run update-chromium.yml successfully first.
  exit /b 1
)

cd /d "%CHROMIUM_SRC%"
if "%WAVE_CHROMIUM_PREPARED%"=="1" goto chromium_ready

call "%DEPOT_TOOLS%\gclient.bat" sync --nohooks
if errorlevel 1 exit /b 1

:chromium_ready
call "%DEPOT_TOOLS%\gclient.bat" runhooks
if errorlevel 1 exit /b 1

python "%~dp0apply_wave_ui.py"
if errorlevel 1 exit /b 1

if not exist out\WaveWin mkdir out\WaveWin

set "WAVE_GN_ARGS=is_debug=false is_component_build=false target_cpu=\"x64\""
if defined SCCACHE_PATH set "WAVE_GN_ARGS=%WAVE_GN_ARGS% cc_wrapper=\"%SCCACHE_PATH%\""
gn gen out\WaveWin --args="%WAVE_GN_ARGS%"
if errorlevel 1 exit /b 1

autoninja -C out\WaveWin chrome
if errorlevel 1 exit /b 1

if not exist "%WAVE_OUT%" mkdir "%WAVE_OUT%"
copy /Y out\WaveWin\chrome.exe "%WAVE_OUT%\WaveBrowser.exe" >nul
if errorlevel 1 exit /b 1

echo.
echo WaveBrowser.exe created at:
echo %WAVE_OUT%\WaveBrowser.exe
