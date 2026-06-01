// WaveBrowserRemote.cpp — Aroma (WUPS) plugin
// Hooks VPAD_BUTTON_TV system-wide and sends HDMI-CEC commands to the TV.
// Works through the HDMI cable — no line of sight needed.
//
// Install: /vol/external01/wiiu/environments/aroma/plugins/WaveBrowserRemote.wps
// Config : /vol/external01/wiiu/apps/WaveBrowser/wave-browser-remote.cfg

#include <wups.h>
#include <vpad/input.h>
#include <coreinit/mutex.h>
#include <coreinit/filesystem_fsa.h>
#include <string.h>
#include <stdio.h>

#include "plugin_config.h"

WUPS_PLUGIN_NAME("WaveBrowserRemote");
WUPS_PLUGIN_DESCRIPTION("Controls TV via HDMI-CEC when TV button is pressed");
WUPS_PLUGIN_VERSION("2.0.0");
WUPS_PLUGIN_AUTHOR("GameBuster");
WUPS_PLUGIN_LICENSE("MIT");
WUPS_USE_WUT_DEVOPTAB();

#define CEC_INITIATOR   0x40   // (4 << 4) | 0  Playback Device -> TV
#define CEC_STANDBY     0x36   // Standby (turn off)
#define CEC_IMAGE_ON    0x04   // Image View On (turn on)
#define CEC_GIVE_POWER  0x8F   // Give Device Power Status
#define CEC_REPORT_PWR  0x90   // Report Power Status
#define CEC_PWR_ON      0x00   // Power status: On
#define CEC_PWR_STANDBY 0x01   // Power status: Standby

#if __has_include(<acp/acp.h>)
#  include <acp/acp.h>
#  define HAVE_ACP 1
#else
#  define HAVE_ACP 0
#endif

// FIX 2: Fail loudly at compile time if ACP is unavailable.
// Without it the plugin compiles but does nothing.
// Add wiiu-acp via dkp-pacman or add -I$(DEVKITPRO)/portlibs/wiiu/include
// to CXXFLAGS in the plugin Makefile if this fires.
#if !HAVE_ACP
#error "acp/acp.h not found — CEC plugin will be a no-op. Install wiiu-acp or fix include path."
#endif

static bool cec_send(const uint8_t* msg, int len)
{
    return ACPSendCECCommand((uint8_t*)msg, (uint32_t)len) == 0;
}

static int cec_recv(uint8_t* buf, int maxlen, uint32_t timeout_ms)
{
    return ACPReceiveCECCommand(buf, maxlen, timeout_ms);
}

// FIX 1: Query TV power state, then send the appropriate command.
// CEC_STANDBY alone is a one-way command that can only turn the TV off.
// To turn it on you need Image View On (0x04).
// We query first with Give Device Power Status (0x8F), wait for the
// Report Power Status response (0x90), then branch.
// If the query times out (TV off / no CEC response) we assume standby
// and send Image View On.
static void cec_power_toggle()
{
    uint8_t query[2] = { CEC_INITIATOR, CEC_GIVE_POWER };
    cec_send(query, 2);

    // Wait up to 300 ms for Report Power Status (0x90)
    uint8_t resp[4] = {};
    int n = cec_recv(resp, sizeof(resp), 300);

    bool tv_is_on = false;
    if (n >= 3 && resp[1] == CEC_REPORT_PWR) {
        // resp[2] = power status byte
        tv_is_on = (resp[2] == CEC_PWR_ON);
    }
    // If query timed out or no response, TV is likely in standby —
    // treat as off and attempt to wake it.

    if (tv_is_on) {
        uint8_t standby[2] = { CEC_INITIATOR, CEC_STANDBY };
        cec_send(standby, 2);
    } else {
        uint8_t wake[2] = { CEC_INITIATOR, CEC_IMAGE_ON };
        cec_send(wake, 2);
    }
}

static TVPluginConfig s_cfg        = {};
static bool           s_cfg_loaded = false;
static OSMutex        s_mutex;

static void load_config()
{
    FILE* f = fopen(PLUGIN_CONFIG_PATH, "rb");
    if (!f) { s_cfg_loaded = false; return; }
    TVPluginConfig tmp = {};
    size_t n = fread(&tmp, 1, sizeof(tmp), f);
    fclose(f);
    if (n != sizeof(tmp) || tmp.magic != PLUGIN_CONFIG_MAGIC)
        { s_cfg_loaded = false; return; }
    OSLockMutex(&s_mutex);
    memcpy(&s_cfg, &tmp, sizeof(s_cfg));
    s_cfg_loaded = (tmp.cec_enabled != 0);
    OSUnlockMutex(&s_mutex);
}

DECL_FUNCTION(int32_t, VPADRead,
              VPADChan chan, VPADStatus* buffers,
              uint32_t count, VPADReadError* outError)
{
    int32_t result = real_VPADRead(chan, buffers, count, outError);
    if (result > 0 && chan == VPAD_CHAN_0 && count > 0) {
        if (buffers[0].trigger & VPAD_BUTTON_TV) {
            load_config();
            if (s_cfg_loaded) {
                cec_power_toggle();
                // Suppress system IR blast — we handled it via CEC
                buffers[0].trigger &= ~VPAD_BUTTON_TV;
                buffers[0].hold    &= ~VPAD_BUTTON_TV;
            }
        }
    }
    return result;
}
WUPS_MUST_REPLACE(VPADRead, WUPS_LOADER_LIBRARY_VPAD, VPADRead);

INITIALIZE_PLUGIN()  { OSInitMutex(&s_mutex); load_config(); }
DEINITIALIZE_PLUGIN() {}
ON_APPLICATION_START() { load_config(); }
