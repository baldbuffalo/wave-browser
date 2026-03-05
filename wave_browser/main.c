#include <proc_ui/procui.h>
#include <coreinit/foreground.h>
#include <coreinit/screen.h>
#include <coreinit/cache.h>
#include <vpad/input.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>

static void SaveCallback(void) {
    OSSavesDone_ReadyToRelease();
}

static void *tvBuffer  = NULL;
static void *drcBuffer = NULL;
static int inForeground = 0;

static void acquireForeground(void) {
    OSScreenInit();
    size_t tvSize  = OSScreenGetBufferSizeEx(SCREEN_TV);
    size_t drcSize = OSScreenGetBufferSizeEx(SCREEN_DRC);
    tvBuffer  = memalign(0x100, tvSize);
    drcBuffer = memalign(0x100, drcSize);
    memset(tvBuffer,  0, tvSize);
    memset(drcBuffer, 0, drcSize);
    OSScreenSetBufferEx(SCREEN_TV,  tvBuffer);
    OSScreenSetBufferEx(SCREEN_DRC, drcBuffer);
    OSScreenEnableEx(SCREEN_TV,  1);
    OSScreenEnableEx(SCREEN_DRC, 1);
    inForeground = 1;
}

static void releaseForeground(void) {
    OSScreenEnableEx(SCREEN_TV,  0);
    OSScreenEnableEx(SCREEN_DRC, 0);
    free(tvBuffer);
    free(drcBuffer);
    tvBuffer = drcBuffer = NULL;
    inForeground = 0;
    ProcUIDrawDoneRelease();
}

static void drawFrame(const char *msg) {
    size_t tvSize  = OSScreenGetBufferSizeEx(SCREEN_TV);
    size_t drcSize = OSScreenGetBufferSizeEx(SCREEN_DRC);
    OSScreenClearBufferEx(SCREEN_TV,  0x00000000);
    OSScreenClearBufferEx(SCREEN_DRC, 0x00000000);
    OSScreenPutFontEx(SCREEN_TV,  0, 0, msg);
    OSScreenPutFontEx(SCREEN_DRC, 0, 0, msg);
    DCFlushRange(tvBuffer,  tvSize);
    DCFlushRange(drcBuffer, drcSize);
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

int main(void) {
    ProcUIInit(&SaveCallback);
    VPADInit();

    while (1) {
        ProcUIStatus status = ProcUIProcessMessages(TRUE);

        if (status == PROCUI_STATUS_EXITING) {
            break;
        } else if (status == PROCUI_STATUS_RELEASE_FOREGROUND) {
            if (inForeground) releaseForeground();
        } else if (status == PROCUI_STATUS_IN_FOREGROUND) {
            if (!inForeground) acquireForeground();
            drawFrame("Wave Browser running! Press HOME to exit.");
            VPADStatus vpad;
            VPADReadError error;
            VPADRead(VPAD_CHAN_0, &vpad, 1, &error);
            usleep(16000);
        }
    }

    if (inForeground) {
        OSScreenEnableEx(SCREEN_TV,  0);
        OSScreenEnableEx(SCREEN_DRC, 0);
        free(tvBuffer);
        free(drcBuffer);
    }
    ProcUIShutdown();
    return 0;
}
