// TV framebuffer capture adapted from Wii-Sec-U wsu-stream.
// Original GX2 capture architecture: DoperDodge/Wii-Sec-U contributors.
// GPL-3.0-or-later; this adapted file remains GPL-3.0-or-later.
#include <cstring>
#include <gx2/enum.h>
#include <gx2/mem.h>
#include <gx2/surface.h>
#include <gx2/swap.h>
#include <wups.h>
#include "mirror_state.h"
#include "platform.h"
namespace mirror_screen {
namespace {
enum SlotState { SLOT_FREE=0,SLOT_COPY_PENDING=1,SLOT_ENCODING=2 };
struct CaptureSlot {GX2Surface surface{};void* image=nullptr;SlotState state=SLOT_FREE;uint32_t issuedAtMs=0;};
OSMutex gCaptureMutex;
CaptureSlot gSlots[2];
CapturedSurface gCaptured[2];
uint32_t gLastCaptureMs=0;
bool gInitialized=false,gFormatWarned=false;
bool supportedFormat(uint32_t f){return f==GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8||f==GX2_SURFACE_FORMAT_SRGB_R8_G8_B8_A8;}
void freeSlot(CaptureSlot& slot){if(slot.image){MEMFreeToDefaultHeap(slot.image);slot.image=nullptr;}slot.state=SLOT_FREE;}
bool prepareSlot(CaptureSlot& slot,const GX2Surface& src){
 if(slot.image&&slot.surface.width==src.width&&slot.surface.height==src.height&&slot.surface.format==src.format)return true;
 freeSlot(slot);std::memset(&slot.surface,0,sizeof(slot.surface));
 slot.surface.dim=GX2_SURFACE_DIM_TEXTURE_2D;slot.surface.width=src.width;slot.surface.height=src.height;
 slot.surface.depth=1;slot.surface.mipLevels=1;slot.surface.format=src.format;slot.surface.aa=GX2_AA_MODE1X;
 slot.surface.use=GX2_SURFACE_USE_TEXTURE;slot.surface.tileMode=GX2_TILE_MODE_LINEAR_ALIGNED;slot.surface.swizzle=0;
 GX2CalcSurfaceSizeAndAlignment(&slot.surface);
 slot.image=MEMAllocFromDefaultHeapEx(slot.surface.imageSize,slot.surface.alignment);
 if(!slot.image){MIRROR_LOG("capture allocation failed: %u bytes",slot.surface.imageSize);return false;}
 slot.surface.image=slot.image;DCFlushRange(slot.image,slot.surface.imageSize);return true;
}
void onTvScanout(const GX2ColorBuffer* cb){
 if(!gInitialized||!cb||!clientActive())return;
 const GX2Surface& src=cb->surface;
 if(src.aa!=GX2_AA_MODE1X)return;
 if(!supportedFormat(src.format)){if(!gFormatWarned){MIRROR_LOG("unsupported TV framebuffer format 0x%x",static_cast<unsigned>(src.format));gFormatWarned=true;}return;}
 OSLockMutex(&gCaptureMutex);uint32_t now=mirrorNowMs();
 for(int i=0;i<2;i++){
  CaptureSlot& slot=gSlots[i];
  if(slot.state==SLOT_COPY_PENDING&&slot.issuedAtMs!=now){
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU,slot.image,slot.surface.imageSize);
   gCaptured[i].pixels=static_cast<const uint8_t*>(slot.image);
   gCaptured[i].width=slot.surface.width;gCaptured[i].height=slot.surface.height;
   gCaptured[i].pitch=slot.surface.pitch;gCaptured[i].format=slot.surface.format;
   gCaptured[i].timestampMs=slot.issuedAtMs;slot.state=SLOT_ENCODING;encoderSubmit(i);
  }
 }
 // Conservative capture rate: gameplay performance takes priority.
 if(static_cast<uint32_t>(now-gLastCaptureMs)>=66u){
  for(int i=0;i<2;i++){CaptureSlot& slot=gSlots[i];if(slot.state!=SLOT_FREE)continue;
   if(!prepareSlot(slot,src))break;
   GX2CopySurface(&src,0,0,&slot.surface,0,0);slot.state=SLOT_COPY_PENDING;
   slot.issuedAtMs=now;gLastCaptureMs=now;break;
  }
 }
 OSUnlockMutex(&gCaptureMutex);
}
}
const CapturedSurface* captureGetSurface(int i){return i<0||i>1?nullptr:&gCaptured[i];}
void captureRelease(int i){if(i<0||i>1)return;OSLockMutex(&gCaptureMutex);gSlots[i].state=SLOT_FREE;OSUnlockMutex(&gCaptureMutex);}
bool captureInit(){OSInitMutex(&gCaptureMutex);gLastCaptureMs=0;gFormatWarned=false;gInitialized=true;return true;}
void captureShutdown(){if(!gInitialized)return;OSLockMutex(&gCaptureMutex);gInitialized=false;for(auto& slot:gSlots)freeSlot(slot);OSUnlockMutex(&gCaptureMutex);}
}
DECL_FUNCTION(void,GX2CopyColorBufferToScanBuffer,const GX2ColorBuffer* colorBuffer,GX2ScanTarget scanTarget);
void my_GX2CopyColorBufferToScanBuffer(const GX2ColorBuffer* colorBuffer,GX2ScanTarget scanTarget){
 if(scanTarget==GX2_SCAN_TARGET_TV)mirror_screen::onTvScanout(colorBuffer);
 real_GX2CopyColorBufferToScanBuffer(colorBuffer,scanTarget);
}
WUPS_MUST_REPLACE(GX2CopyColorBufferToScanBuffer,WUPS_LOADER_LIBRARY_GX2,GX2CopyColorBufferToScanBuffer);
