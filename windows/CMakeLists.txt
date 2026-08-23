cmake_minimum_required(VERSION 3.20)
project(WaveBrowser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(WaveBrowser WIN32 WaveBrowser.cpp WaveAddressBar.cpp)
target_compile_definitions(WaveBrowser PRIVATE UNICODE _UNICODE)

if (MSVC)
  target_compile_options(WaveBrowser PRIVATE /W4)
endif()
