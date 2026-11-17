#pragma once
#include <stdint.h>

#define PLUGIN_CONFIG_PATH \
    "fs:/vol/external01/wiiu/apps/WaveBrowser/wave-browser-remote.cfg"
#define PLUGIN_CONFIG_MAGIC   0x57425456u
#define PLUGIN_CONFIG_VERSION 2

#pragma pack(push, 1)
struct TVPluginConfig {
    uint32_t magic;
    uint8_t  version;
    uint8_t  cec_enabled;  // 1 = CEC active
    uint8_t  _pad[2];
    char     brand[32];
    char     model[32];
    uint16_t year;
};
#pragma pack(pop)
