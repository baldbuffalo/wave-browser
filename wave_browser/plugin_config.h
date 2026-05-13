#pragma once
#include <stdint.h>

// ─── Shared between Wave Browser app and WaveBrowserRemote Aroma plugin ───────
// The app writes this file when the user selects a TV model.
// The plugin reads it on load + on every TV button press.

#define PLUGIN_CONFIG_PATH \
    "fs:/vol/external01/wiiu/apps/WaveBrowser/wave-browser-remote.cfg"

#define PLUGIN_CONFIG_MAGIC  0x57425456u   // "WBTV"
#define PLUGIN_CONFIG_VERSION 1

// IR protocol IDs — must match IRProtocol enum in tv_remote.h
#define PROTO_NONE      0
#define PROTO_NEC       1
#define PROTO_SAMSUNG   2
#define PROTO_SIRCS_12  3
#define PROTO_SIRCS_15  4
#define PROTO_SIRCS_20  5
#define PROTO_RC5       6
#define PROTO_RC6       7
#define PROTO_PANASONIC 8
#define PROTO_JVC       9
#define PROTO_ROKU      10

// Button slot indices — must match TVBtn enum order in tv_remote.h
#define SLOT_POWER     0
#define SLOT_INPUT     1
#define SLOT_MUTE      2
#define SLOT_VOL_UP    3
#define SLOT_VOL_DOWN  4
#define SLOT_CH_UP     5
#define SLOT_CH_DOWN   6
#define SLOT_COUNT     7   // plugin only needs the top 7 — rest unused

// ─── On-disk config (raw binary, little-endian) ───────────────────────────────

#pragma pack(push, 1)
struct TVPluginConfig {
    uint32_t magic;           // PLUGIN_CONFIG_MAGIC
    uint8_t  version;         // PLUGIN_CONFIG_VERSION
    uint8_t  protocol;        // PROTO_* above
    uint8_t  _pad[2];
    char     brand[32];       // e.g. "Samsung"
    char     model[32];       // e.g. "UE40H6400"
    uint16_t year;

    // IR codes for the top 7 buttons (power, input, mute, vol+, vol-, ch+, ch-)
    // code == 0 means unsupported on this model
    struct {
        uint32_t code;
        uint8_t  repeats;
        uint8_t  _pad[3];
    } ir[SLOT_COUNT];
};
#pragma pack(pop)
