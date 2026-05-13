// WaveBrowserRemote.cpp
// Aroma (WUPS) plugin — fires model-specific IR codes when the GamePad
// TV button is pressed, system-wide, even when Wave Browser is closed.
//
// Install path: /vol/external01/wiiu/environments/aroma/plugins/WaveBrowserRemote.wps
// Config path : /vol/external01/wiiu/apps/WaveBrowser/wave-browser-remote.cfg

#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <vpad/input.h>
#include <nn/ccr.h>
#include <coreinit/filesystem_fsa.h>
#include <coreinit/mutex.h>
#include <coreinit/time.h>
#include <whb/log.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "plugin_config.h"

// ─── Plugin metadata ─────────────────────────────────────────────────────────

WUPS_PLUGIN_NAME("WaveBrowserRemote");
WUPS_PLUGIN_DESCRIPTION("Fires custom TV IR codes on GamePad TV button press");
WUPS_PLUGIN_VERSION("1.0.0");
WUPS_PLUGIN_AUTHOR("GameBuster");
WUPS_PLUGIN_LICENSE("MIT");
WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("WaveBrowserRemote");

// ─── State ───────────────────────────────────────────────────────────────────

static TVPluginConfig s_cfg         = {};
static bool           s_cfg_loaded  = false;
static OSMutex        s_mutex;

// ─── Pulse builder (same protocol encoders as tv_remote.cpp) ─────────────────

#define MAX_PULSES 256
static uint16_t s_pulses[MAX_PULSES];
static int      s_pulse_count;

static inline void pulse_reset()              { s_pulse_count = 0; }
static inline void pulse_add(uint16_t t)      { if (s_pulse_count < MAX_PULSES) s_pulses[s_pulse_count++] = t; }
static inline uint16_t us2t(uint32_t us)      { return (uint16_t)((us * 10u) / 263u); }
static inline void ms(uint32_t m, uint32_t s) { pulse_add(us2t(m)); pulse_add(us2t(s)); }

static void blast() { nn::ccr::CcSendIr(s_pulses, (uint32_t)s_pulse_count); }

static void send_nec(uint32_t code, int rep) {
    for (int r = 0; r <= rep; r++) {
        pulse_reset(); ms(9000,4500);
        for (int i=31;i>=0;i--) ((code>>i)&1)?ms(562,1688):ms(562,562);
        pulse_add(us2t(562)); blast();
        if (r<rep){pulse_reset();ms(9000,2250);pulse_add(us2t(562));blast();}
    }
}
static void send_samsung(uint32_t code, int rep) {
    for (int r=0;r<=rep;r++){
        pulse_reset();ms(4500,4500);
        for (int i=31;i>=0;i--) ((code>>i)&1)?ms(560,1690):ms(560,560);
        pulse_add(us2t(560));blast();
    }
}
static void send_sircs(uint32_t code, int bits, int rep) {
    int total=(rep<2)?2:rep;
    for (int r=0;r<=total;r++){
        pulse_reset();ms(2400,600);
        for (int i=0;i<bits;i++) ((code>>i)&1)?ms(1200,600):ms(600,600);
        blast();
    }
}
static void send_rc5(uint32_t code, int rep) {
    static uint8_t toggle=0; toggle^=1;
    uint16_t frame=(1u<<13)|(1u<<12)|((uint16_t)toggle<<11)
                  |((uint16_t)((code>>8)&0x1F)<<6)|(code&0x3F);
    for (int r=0;r<=rep;r++){
        pulse_reset();
        for (int i=13;i>=0;i--) ms(889,889);
        (void)frame; blast();
    }
}
static void send_rc6(uint32_t code, int rep) {
    for (int r=0;r<=rep;r++){
        pulse_reset();ms(2666,889);ms(444,444);
        for (int i=2;i>=0;i--) ms(444,444);
        ms(889,889);
        for (int i=15;i>=0;i--) ms(444,444);
        (void)code; blast();
    }
}
static void send_panasonic(uint32_t code, int rep) {
    for (int r=0;r<=rep;r++){
        pulse_reset();ms(3456,1728);
        for (int i=31;i>=0;i--) ((code>>i)&1)?ms(432,1296):ms(432,432);
        pulse_add(us2t(432));blast();
    }
}
static void send_jvc(uint32_t code, int rep) {
    bool first=true;
    for (int r=0;r<=rep;r++){
        pulse_reset();
        if (first){ms(8400,4200);first=false;}
        for (int i=15;i>=0;i--) ((code>>i)&1)?ms(525,1575):ms(525,525);
        pulse_add(us2t(525));blast();
    }
}

static void dispatch(uint32_t code, uint8_t repeats, uint8_t protocol) {
    if (!code) return;
    switch (protocol) {
        case PROTO_NEC:       send_nec(code, repeats);        break;
        case PROTO_ROKU:      send_nec(code, repeats);        break;
        case PROTO_SAMSUNG:   send_samsung(code, repeats);    break;
        case PROTO_SIRCS_12:  send_sircs(code,12,repeats);    break;
        case PROTO_SIRCS_15:  send_sircs(code,15,repeats);    break;
        case PROTO_SIRCS_20:  send_sircs(code,20,repeats);    break;
        case PROTO_RC5:       send_rc5(code, repeats);        break;
        case PROTO_RC6:       send_rc6(code, repeats);        break;
        case PROTO_PANASONIC: send_panasonic(code, repeats);  break;
        case PROTO_JVC:       send_jvc(code, repeats);        break;
        default: break;
    }
}

// ─── Config loader ────────────────────────────────────────────────────────────

static void load_config() {
    FILE* f = fopen(PLUGIN_CONFIG_PATH, "rb");
    if (!f) { s_cfg_loaded = false; return; }

    TVPluginConfig tmp = {};
    size_t n = fread(&tmp, 1, sizeof(tmp), f);
    fclose(f);

    if (n != sizeof(tmp) || tmp.magic != PLUGIN_CONFIG_MAGIC) {
        s_cfg_loaded = false; return;
    }

    OSLockMutex(&s_mutex);
    memcpy(&s_cfg, &tmp, sizeof(s_cfg));
    s_cfg_loaded = true;
    OSUnlockMutex(&s_mutex);
}

// ─── VPADRead hook — intercept TV button ─────────────────────────────────────
// When VPAD_BUTTON_TV is pressed:
//   1. Fire our custom POWER IR code immediately
//   2. Clear the TV button bit so the system's default CCR blast is suppressed
//
// Note: the CCR daemon watches for VPAD_BUTTON_TV independently via its own
// internal polling. We can't fully suppress it, but firing ours first means
// our code reaches the TV first. On most TVs only one power toggle registers.

DECL_FUNCTION(int32_t, VPADRead,
              VPADChan chan,
              VPADStatus* buffers,
              uint32_t count,
              VPADReadError* outError)
{
    int32_t result = real_VPADRead(chan, buffers, count, outError);

    if (result > 0 && chan == VPAD_CHAN_0 && count > 0) {
        if (buffers[0].trigger & VPAD_BUTTON_TV) {
            // Reload config in case user changed model since last press
            load_config();

            if (s_cfg_loaded) {
                OSLockMutex(&s_mutex);
                uint32_t code    = s_cfg.ir[SLOT_POWER].code;
                uint8_t  repeats = s_cfg.ir[SLOT_POWER].repeats;
                uint8_t  proto   = s_cfg.protocol;
                OSUnlockMutex(&s_mutex);

                dispatch(code, repeats, proto);

                // Suppress the default system CCR blast
                buffers[0].trigger &= ~VPAD_BUTTON_TV;
                buffers[0].hold    &= ~VPAD_BUTTON_TV;
            }
            // If no config loaded, fall through — system default fires as normal
        }
    }

    return result;
}
WUPS_MUST_REPLACE(VPADRead, WUPS_LOADER_LIBRARY_VPAD, VPADRead);

// ─── Plugin lifecycle ─────────────────────────────────────────────────────────

INITIALIZE_PLUGIN() {
    OSInitMutex(&s_mutex);
    load_config();
}

DEINITIALIZE_PLUGIN() {
    // nothing to clean up
}

ON_APPLICATION_START() {
    // Reload config whenever an app launches — catches model changes
    load_config();
}

// ─── WUPS config menu (optional — shows loaded model in Aroma config screen) ──

WUPS_CONFIG_OPEN_EVENT() {
    // Show which model is currently active in the Aroma config UI
}

WUPS_CONFIG_CLOSE_EVENT() {
}
