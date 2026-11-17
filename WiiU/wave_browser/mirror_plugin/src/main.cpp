// MirrorScreen standalone Aroma plugin: browser-based TV mirroring over local Wi-Fi.
#include <cstdio>
#include <wups.h>
#include <wups/storage.h>
#include "platform.h"
#include "mirror_state.h"
WUPS_PLUGIN_NAME("MirrorScreen");
WUPS_PLUGIN_DESCRIPTION("Streams Wii U TV video over Wi-Fi to a web browser");
WUPS_PLUGIN_VERSION("0.1.0");
WUPS_PLUGIN_AUTHOR("GameBuster");
WUPS_PLUGIN_LICENSE("GPLv3-or-later");
WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("mirror_screen");
INITIALIZE_PLUGIN() {
 WHBLogCafeInit();
 FILE* f=std::fopen("fs:/vol/external01/wiiu/apps/WaveBrowser/mirror-screen-status.cfg","w");
 if(f){std::fputs("loaded=0\nactive=0\nsource=TV\ndevice=\n",f);std::fclose(f);}
}
ON_APPLICATION_START() {
 WHBLogCafeInit();
 if(!mirror_screen::httpStart()){MIRROR_LOG("HTTP receiver failed to start");return;}
 mirror_screen::captureInit();
 if(!mirror_screen::encoderStart()){
  MIRROR_LOG("JPEG encoder failed to start");mirror_screen::captureShutdown();mirror_screen::httpStop();return;
 }
 MIRROR_LOG("ready — browse to http://WIIU-IP:8088 on the same Wi-Fi");
}
ON_APPLICATION_ENDS() {
 mirror_screen::httpStop();mirror_screen::encoderStop();mirror_screen::captureShutdown();
}
DEINITIALIZE_PLUGIN() {
 mirror_screen::httpStop();mirror_screen::encoderStop();mirror_screen::captureShutdown();
}
