#include <vpad/input.h>
#include <unistd.h>

int main(void) {
    VPADInit();

    while (1) {
        VPADStatus vpad;
        VPADReadError error;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &error);

        if (vpad.trigger & VPAD_BUTTON_HOME) break;

        usleep(16000);
    }

    return 0;
}
