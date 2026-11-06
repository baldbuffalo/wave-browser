// Software JPEG encoder adapted from Wii-Sec-U wsu-stream.
// Original design: DoperDodge/Wii-Sec-U contributors, GPL-3.0-or-later.
// This adapted file remains GPL-3.0-or-later.
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <coreinit/messagequeue.h>
#include <gx2/enum.h>
#include <jpeglib.h>
#include "mirror_state.h"
#include "platform.h"
namespace mirror_screen {
namespace {
constexpr uint32_t kStackSize=512*1024;
constexpr int32_t kPriority=28;
constexpr int kQueueSize=4;
constexpr int32_t kShutdown=-1;
std::atomic<bool> gRunning{false};
OSMessageQueue gQueue;
OSMessage gQueueBuffer[kQueueSize];
MirrorThread gThread;
uint8_t* gRgb=nullptr;
unsigned char* gJpeg=nullptr;
unsigned long gJpegCapacity=0;
void downscale(const CapturedSurface& src){
 const uint32_t* pixels=reinterpret_cast<const uint32_t*>(src.pixels);
 for(int y=0;y<kHeight;y++){
  uint32_t sy=static_cast<uint32_t>(y)*src.height/kHeight;
  const uint32_t* row=pixels+static_cast<size_t>(sy)*src.pitch;
  uint8_t* out=gRgb+static_cast<size_t>(y)*kWidth*3;
  for(int x=0;x<kWidth;x++){
   uint32_t sx=static_cast<uint32_t>(x)*src.width/kWidth,px=row[sx];
   out[x*3]=static_cast<uint8_t>(px>>24);out[x*3+1]=static_cast<uint8_t>(px>>16);out[x*3+2]=static_cast<uint8_t>(px>>8);
  }
 }
}
size_t compressJpeg(){
 jpeg_compress_struct info{};jpeg_error_mgr error{};info.err=jpeg_std_error(&error);jpeg_create_compress(&info);
 unsigned char* output=gJpeg;unsigned long outputSize=gJpegCapacity;jpeg_mem_dest(&info,&output,&outputSize);
 info.image_width=kWidth;info.image_height=kHeight;info.input_components=3;info.in_color_space=JCS_RGB;
 jpeg_set_defaults(&info);jpeg_set_quality(&info,58,TRUE);info.dct_method=JDCT_IFAST;
 jpeg_start_compress(&info,TRUE);
 while(info.next_scanline<info.image_height){JSAMPROW row=gRgb+static_cast<size_t>(info.next_scanline)*kWidth*3;jpeg_write_scanlines(&info,&row,1);}
 jpeg_finish_compress(&info);jpeg_destroy_compress(&info);
 if(output!=gJpeg){size_t copied=outputSize<=gJpegCapacity?outputSize:0;if(copied)std::memcpy(gJpeg,output,copied);std::free(output);return copied;}
 return outputSize;
}
int encoderEntry(int,void*){
 gRgb=new(std::nothrow) uint8_t[static_cast<size_t>(kWidth)*kHeight*3];
 gJpegCapacity=static_cast<unsigned long>(kJpegCapacity);gJpeg=static_cast<unsigned char*>(std::malloc(gJpegCapacity));
 if(!gRgb||!gJpeg){MIRROR_LOG("encoder buffer allocation failed");delete[] gRgb;gRgb=nullptr;std::free(gJpeg);gJpeg=nullptr;return -1;}
 while(gRunning.load()){
  OSMessage msg{};OSReceiveMessage(&gQueue,&msg,OS_MESSAGE_FLAGS_BLOCKING);
  int32_t index=static_cast<int32_t>(msg.args[0]);if(index==kShutdown)break;
  const CapturedSurface* src=captureGetSurface(index);
  if(!src||!src->pixels){captureRelease(index);continue;}
  downscale(*src);size_t size=compressJpeg();captureRelease(index);
  if(size>0)publishFrame(gJpeg,size);
 }
 delete[] gRgb;gRgb=nullptr;std::free(gJpeg);gJpeg=nullptr;gJpegCapacity=0;return 0;
}
}
bool encoderSubmit(int i){OSMessage msg{};msg.args[0]=static_cast<uint32_t>(i);return OSSendMessage(&gQueue,&msg,OS_MESSAGE_FLAGS_NONE);}
bool encoderStart(){
 if(gRunning.load())return true;OSInitMessageQueue(&gQueue,gQueueBuffer,kQueueSize);gRunning.store(true);
 if(!gThread.start(encoderEntry,kStackSize,kPriority,OS_THREAD_ATTRIB_AFFINITY_CPU2,"MirrorScreen encoder")){gRunning.store(false);MIRROR_LOG("could not start JPEG encoder");return false;}
 return true;
}
void encoderStop(){if(!gRunning.exchange(false))return;OSMessage msg{};msg.args[0]=static_cast<uint32_t>(kShutdown);OSSendMessage(&gQueue,&msg,OS_MESSAGE_FLAGS_BLOCKING);gThread.join();}
}
