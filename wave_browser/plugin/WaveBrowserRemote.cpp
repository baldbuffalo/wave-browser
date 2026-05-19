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
#define CEC_STANDBY     0x36   // Power toggle (works on all CEC TVs)

#if __has_include(<acp/acp.h>)
#  include <acp/acp.h>
#  define HAVE_ACP 1
#else
#  define HAVE_ACP 0
#endif

static bool cec_send(const uint8_t* msg, int len)
{
#if HAVE_ACP
    return ACPSendCECCommand((uint8_t*)msg, (uint32_t)len) == 0;
#else
    (void)msg; (void)len; return false;
#endif
}

static void cec_power_toggle()
{
    uint8_t msg[2] = { CEC_INITIATOR, CEC_STANDBY };
    cec_send(msg, 2);
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
                // Suppress system IR blast
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
