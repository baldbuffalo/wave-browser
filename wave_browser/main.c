#include <proc_ui/procui.h>
#include <coreinit/foreground.h>
#include <vpad/input.h>
#include <unistd.h>

static void SaveCallback(void) {
    OSSavesDone_ReadyToRelease();
}

int main(void) {
    ProcUIInit(&SaveCallback);
    VPADInit();

    while (ProcUIIsRunning()) {
        VPADStatus vpad;
        VPADReadError error;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &error);
        ProcUIProcessMessages(TRUE);
        usleep(16000);
    }

    ProcUIShutdown();
    return 0;
}
