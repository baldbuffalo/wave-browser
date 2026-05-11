#pragma once
#include "tv_remote.h"

// ─── HDMI-CEC auto-detection ─────────────────────────────────────────────────
// Runs a background thread that sends CEC probes to the TV (logical addr 0),
// reads back Vendor ID + OSD name, then scores every registered model.
// Called from settings.cpp when the user hits SELECT on the TV Remote row.

struct TVDetectResult {
    bool                 found;
    uint32_t             cec_vendor;
    char                 osd_name[16];
    const TVRemoteModel* model;   // best match, or nullptr
};

void                  tv_detect_start();          // non-blocking, spawns thread
bool                  tv_detect_done();           // poll each frame
const TVDetectResult* tv_detect_result();         // valid after done() == true
int                   tv_detect_score(const TVRemoteModel*, const TVDetectResult*);
