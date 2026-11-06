#pragma once
#include <cstddef>
#include <cstdint>
namespace mirror_screen {
constexpr int kWidth=428,kHeight=240,kJpegCapacity=kWidth*kHeight*3;
constexpr uint16_t kHttpPort=8088;
struct CapturedSurface {const uint8_t* pixels=nullptr;uint32_t width=0,height=0,pitch=0,format=0,timestampMs=0;};
bool captureInit(); void captureShutdown();
bool encoderStart(); void encoderStop(); void encoderSubmit(int); void captureRelease(int);
const CapturedSurface* captureGetSurface(int);
bool httpStart(); void httpStop(); bool clientActive(); void publishFrame(const uint8_t*,size_t);
}
