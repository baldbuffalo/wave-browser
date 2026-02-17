#include <whb/log.h>
#include <whb/application.h>
#include <vpad/input.h>
#include <gx2/init.h>
#include <gx2/display.h>
#include <gx2/mem.h>
#include <gx2/context.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#define TOPBAR_HEIGHT 80
#define ADDRESSBAR_HEIGHT 50

static GX2ContextState *contextState;

// Simple color clear
void clearScreen(float r, float g, float b, float a) {
    GX2SetClearColor(r, g, b, a);
    GX2ClearColor(GX2_RENDER_TARGET_0, r, g, b, a);
}

void drawUI() {
    // For now we only clear screen different colors to simulate sections

    // Entire background
    clearScreen(1.0f, 1.0f, 1.0f, 1.0f); // white

    // Top bar color overlay (light gray)
    GX2SetScissor(0, 0, SCREEN_WIDTH, TOPBAR_HEIGHT);
    GX2ClearColor(GX2_RENDER_TARGET_0, 0.8f, 0.8f, 0.8f, 1.0f);

    // Address bar area
    GX2SetScissor(150, 15, SCREEN_WIDTH - 300, ADDRESSBAR_HEIGHT);
    GX2ClearColor(GX2_RENDER_TARGET_0, 1.0f, 1.0f, 1.0f, 1.0f);

    // Reset scissor
    GX2SetScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void handleInput() {
    VPADStatus vpad;
    VPADRead(0, &vpad, 1, NULL);

    if (vpad.trigger & VPAD_BUTTON_A) {
        WHBLogPrintf("A pressed\n");
    }
}

int main(void) {

    WHBLogInit();
    GX2Init(NULL);
    VPADInit();

    contextState = (GX2ContextState*)MEMAllocFromDefaultHeap(sizeof(GX2ContextState));
    GX2SetupContextState(contextState);

    WHBLogPrintf("Wave Browser v0.1 Starting...\n");

    while (WHBProcIsRunning()) {

        WHBProcRun();

        GX2SetContextState(contextState);
        GX2SetViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 1.0f);

        drawUI();
        handleInput();

        GX2SwapScanBuffers();
        GX2Flush();
    }

    GX2Shutdown();
    WHBLogDeinit();
    return 0;
}
