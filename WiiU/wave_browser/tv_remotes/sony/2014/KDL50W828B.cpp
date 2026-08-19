// Sony KDL-50W828B (2014) – BRAVIA, SIRCS-20, 40 kHz
// CEC Vendor ID: 0x080046 (Sony)
// SIRCS-20: 7-bit command | 5-bit address | 8-bit extended
// Packed as: extended(8)<<12 | addr(5)<<7 | cmd(7)  [20 bits LSB-first]

#include "../../model_registry.h"

// SIRCS-20 helper: encode extended(8)+addr(5)+cmd(7)
#define S20(ext, addr, cmd) ((uint32_t)(((ext)<<12)|((addr)<<7)|(cmd)))

static const TVRemoteModel SONY_KDL50W828B = {
    .brand = "Sony", .model = "KDL50W828B", .year = 2014,
    .protocol = IRProtocol::SIRCS_20,
    .cec_vendor = 0x080046,
    .cec_osd_name_prefix = "BRAVIA",
    .ir = {
        [TVBTN_POWER]    = { S20(0xA8,0x01,0x15), 2 },
        [TVBTN_INPUT]    = { S20(0xA8,0x01,0x25), 0 },
        [TVBTN_MUTE]     = { S20(0xA8,0x01,0x14), 0 },
        [TVBTN_VOL_UP]   = { S20(0xA8,0x01,0x12), 2 },
        [TVBTN_VOL_DOWN] = { S20(0xA8,0x01,0x13), 2 },
        [TVBTN_CH_UP]    = { S20(0xA8,0x01,0x10), 2 },
        [TVBTN_CH_DOWN]  = { S20(0xA8,0x01,0x11), 2 },
        [TVBTN_0]        = { S20(0xA8,0x01,0x00), 0 },
        [TVBTN_1]        = { S20(0xA8,0x01,0x01), 0 },
        [TVBTN_2]        = { S20(0xA8,0x01,0x02), 0 },
        [TVBTN_3]        = { S20(0xA8,0x01,0x03), 0 },
        [TVBTN_4]        = { S20(0xA8,0x01,0x04), 0 },
        [TVBTN_5]        = { S20(0xA8,0x01,0x05), 0 },
        [TVBTN_6]        = { S20(0xA8,0x01,0x06), 0 },
        [TVBTN_7]        = { S20(0xA8,0x01,0x07), 0 },
        [TVBTN_8]        = { S20(0xA8,0x01,0x08), 0 },
        [TVBTN_9]        = { S20(0xA8,0x01,0x09), 0 },
        [TVBTN_UP]       = { S20(0xA8,0x01,0x74), 0 },
        [TVBTN_DOWN]     = { S20(0xA8,0x01,0x75), 0 },
        [TVBTN_LEFT]     = { S20(0xA8,0x01,0x34), 0 },
        [TVBTN_RIGHT]    = { S20(0xA8,0x01,0x33), 0 },
        [TVBTN_OK]       = { S20(0xA8,0x01,0x65), 0 },
        [TVBTN_BACK]     = { S20(0xA8,0x01,0x6C), 0 },
        [TVBTN_HOME]     = { S20(0xA8,0x01,0x60), 0 },
        [TVBTN_MENU]     = { S20(0xA8,0x01,0x40), 0 },
        [TVBTN_GUIDE]    = { S20(0xA8,0x01,0x46), 0 },
        [TVBTN_INFO]     = { S20(0xA8,0x01,0x5A), 0 },
        [TVBTN_RED]      = { S20(0xA8,0x01,0x6D), 0 },
        [TVBTN_GREEN]    = { S20(0xA8,0x01,0x6E), 0 },
        [TVBTN_YELLOW]   = { S20(0xA8,0x01,0x6F), 0 },
        [TVBTN_BLUE]     = { S20(0xA8,0x01,0x70), 0 },
        [TVBTN_PLAY]     = { S20(0xA8,0x01,0x32), 0 },
        [TVBTN_PAUSE]    = { S20(0xA8,0x01,0x19), 0 },
        [TVBTN_STOP]     = { S20(0xA8,0x01,0x18), 0 },
        [TVBTN_REWIND]   = { S20(0xA8,0x01,0x1B), 0 },
        [TVBTN_FFWD]     = { S20(0xA8,0x01,0x1C), 0 },
    },
    .layout = nullptr, .layout_count = 0,
    .theme = {
        {0x1A,0x1A,0x1A,0xFF},{0x00,0x3C,0x8A,0xFF},  // Sony blue
        {0x00,0x60,0xD0,0xFF},{0x2A,0x2A,0x2A,0xFF},{0xFF,0xFF,0xFF,0xFF}
    }
};
MODEL_REGISTER(SONY_KDL50W828B)
