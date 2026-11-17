// WaveBrowserRemote.cpp — Aroma (WUPS) plugin
// Hooks VPAD_BUTTON_TV system-wide and sends HDMI-CEC commands to the TV.
//
// CEC via OSDynLoad — acp.rpl is always present on WiiU hardware.
// ACPInitialize() must be called before any CEC command works.
// Power state is queried live via Give Device Power Status (0x8F) each press.
//
// Install: /vol/external01/wiiu/environments/aroma/plugins/WaveBrowserRemote.wps
// Config : /vol/external01/wiiu/apps/WaveBrowser/wave-browser-remote.cfg

#include <wups.h>
#include <vpad/input.h>
#include <coreinit/mutex.h>
#include <coreinit/dynload.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
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

// ─── CEC constants ────────────────────────────────────────────────────────────
#define CEC_INITIATOR    0x40   // logical addr 4 (Playback) → addr 0 (TV)
#define CEC_IMAGE_ON     0x04   // Image View On  — wakes TV from standby
#define CEC_STANDBY      0x36   // Standby        — puts TV into standby
#define CEC_GIVE_POWER   0x8F   // Give Device Power Status
#define CEC_REPORT_PWR   0x90   // Report Power Status (response)
#define CEC_PWR_ON       0x00   // Power status byte: On
#define CEC_PWR_TRANS_ON 0x03   // Power status byte: In transition to On

// ─── ACP via OSDynLoad ────────────────────────────────────────────────────────
typedef int32_t (*ACPInitializeFn)       (void);
typedef void    (*ACPFinalizeFn)         (void);
typedef int32_t (*ACPSendCECCommandFn)   (uint8_t* data, uint32_t size);
typedef int32_t (*ACPReceiveCECCommandFn)(uint8_t* data, uint32_t maxSize,
                                          uint32_t timeout_ms);

static OSDynLoad_Module      s_acp_handle = nullptr;
static ACPInitializeFn       s_acp_init   = nullptr;
static ACPFinalizeFn         s_acp_fini   = nullptr;
static ACPSendCECCommandFn   s_acp_send   = nullptr;
static ACPReceiveCECCommandFn s_acp_recv  = nullptr;
static bool                  s_acp_ready  = false;

static bool acp_load()
{
    if (s_acp_ready) return true;

    if (OSDynLoad_Acquire("acp.rpl", &s_acp_handle) != OS_DYNLOAD_OK)
        return false;

    OSDynLoad_FindExport(s_acp_handle, FALSE, "ACPInitialize",
                         (void**)&s_acp_init);
    OSDynLoad_FindExport(s_acp_handle, FALSE, "ACPFinalize",
                         (void**)&s_acp_fini);
    OSDynLoad_FindExport(s_acp_handle, FALSE, "ACPSendCECCommand",
                         (void**)&s_acp_send);
    OSDynLoad_FindExport(s_acp_handle, FALSE, "ACPReceiveCECCommand",
                         (void**)&s_acp_recv);

    if (!s_acp_send || !s_acp_recv) {
        OSDynLoad_Release(s_acp_handle);
        s_acp_handle = nullptr;
        s_acp_send   = nullptr;
        s_acp_recv   = nullptr;
        return false;
    }

    // ACPInitialize MUST be called — CEC commands silently fail without it
    if (s_acp_init) s_acp_init();

    s_acp_ready = true;
    return true;
}

// ─── CEC helpers ──────────────────────────────────────────────────────────────
static bool cec_send(const uint8_t* msg, int len)
{
    return s_acp_send((uint8_t*)msg, (uint32_t)len) == 0;
}

static int cec_recv(uint8_t* buf, int maxlen, uint32_t timeout_ms)
{
    return s_acp_recv(buf, (uint32_t)maxlen, timeout_ms);
}

// ─── CEC power toggle ─────────────────────────────────────────────────────────
// Sends Give Device Power Status (0x8F) to logical address 0 (TV).
// Waits up to 500 ms for Report Power Status (0x90) in response.
// Branches on the reported power state byte:
//   0x00 / 0x03 = on or transitioning on → send Standby (0x36)
//   anything else = standby/off           → send Image View On (0x04)
// If the TV doesn't respond within the timeout it is assumed to be off
// and Image View On is sent.
static void cec_power_toggle()
{
    if (!acp_load()) return;

    // Flush any stale CEC messages sitting in the receive buffer
    // by doing a short drain before sending our query
    {
        uint8_t drain[16];
        while (cec_recv(drain, sizeof(drain), 0) > 0) {}
    }

    uint8_t query[2] = { CEC_INITIATOR, CEC_GIVE_POWER };
    if (!cec_send(query, 2)) return;

    // Poll for Report Power Status — may take a couple of bus cycles
    uint8_t resp[4]  = {};
    bool    got_resp = false;

    // Try up to 3 times with 200 ms each — some TVs are slow to respond
    for (int attempt = 0; attempt < 3 && !got_resp; attempt++) {
        int n = cec_recv(resp, sizeof(resp), 200);
        if (n >= 3 && resp[1] == CEC_REPORT_PWR) {
            got_resp = true;
        }
    }

    bool tv_is_on = false;
    if (got_resp) {
        uint8_t status = resp[2];
        tv_is_on = (status == CEC_PWR_ON || status == CEC_PWR_TRANS_ON);
    }
    // No response within timeout → TV is off or CEC bus not responding → wake it

    if (tv_is_on) {
        uint8_t msg[2] = { CEC_INITIATOR, CEC_STANDBY };
        cec_send(msg, 2);
    } else {
        uint8_t msg[2] = { CEC_INITIATOR, CEC_IMAGE_ON };
        cec_send(msg, 2);
    }
}

// ─── Config ───────────────────────────────────────────────────────────────────
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
    if (n != sizeof(tmp) || tmp.magic != PLUGIN_CONFIG_MAGIC) {
        s_cfg_loaded = false;
        return;
    }
    OSLockMutex(&s_mutex);
    memcpy(&s_cfg, &tmp, sizeof(s_cfg));
    s_cfg_loaded = (tmp.cec_enabled != 0);
    OSUnlockMutex(&s_mutex);
}

// ─── VPADRead hook ────────────────────────────────────────────────────────────
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
                // Suppress system IR blast — TV is controlled via CEC
                buffers[0].trigger &= ~VPAD_BUTTON_TV;
                buffers[0].hold    &= ~VPAD_BUTTON_TV;
            }
        }
    }
    return result;
}
WUPS_MUST_REPLACE(VPADRead, WUPS_LOADER_LIBRARY_VPAD, VPADRead);

INITIALIZE_PLUGIN()
{
    OSInitMutex(&s_mutex);
    load_config();
    acp_load();
}

DEINITIALIZE_PLUGIN()
{
    if (s_acp_ready && s_acp_fini) s_acp_fini();
    if (s_acp_handle) {
        OSDynLoad_Release(s_acp_handle);
        s_acp_handle = nullptr;
    }
    s_acp_ready = false;
    s_acp_send  = nullptr;
    s_acp_recv  = nullptr;
}

ON_APPLICATION_START() { load_config(); }
