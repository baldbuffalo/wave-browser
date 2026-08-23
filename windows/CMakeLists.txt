cmake_minimum_required(VERSION 3.20)
project(WaveBrowser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(WaveBrowser WIN32
    WaveBrowser.cpp
    WaveAddressBar.cpp
    ChromiumHost.cpp
    ChromiumHost.h
)

target_compile_definitions(WaveBrowser PRIVATE UNICODE _UNICODE)

if (MSVC)
  target_compile_options(WaveBrowser PRIVATE /W4)
endif()

# The Chromium checkout/build is supplied by the Wave Chromium build script.
# The generated Chromium desktop target is linked into the final browser build.
