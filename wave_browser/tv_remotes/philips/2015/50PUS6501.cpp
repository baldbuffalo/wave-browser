// Philips 50PUS6501 (2015) – 4K UHD Android TV, RC-6 protocol
// CEC Vendor ID: 0x00903E (Philips / TP Vision)
// RC-6 code packing: mode(3)<<16 | data(16) — we store in code field

#include "../../model_registry.h"

// RC6 mode=0, data = system(8)<<8 | command(8)
// Philips TV system = 0x00
#define RC6(cmd) ((uint32_t)((0<<16)|(0x00<<8)|(cmd)))

static const TVRemoteModel PHILIPS_50PUS6501 = {
    .brand = "Philips", .model = "50PUS6501", .year = 2015,
    .protocol = IRProtocol::RC6,
    .cec_vendor = 0x00903E,
    .cec_osd_name_prefix = "Philips",
    .ir = {
        [TVBTN_POWER]    = { RC6(0x0C), 0 },
        [TVBTN_INPUT]    = { RC6(0x38), 0 },
        [TVBTN_MUTE]     = { RC6(0x0D), 0 },
        [TVBTN_VOL_UP]   = { RC6(0x10), 2 },
        [TVBTN_VOL_DOWN] = { RC6(0x11), 2 },
        [TVBTN_CH_UP]    = { RC6(0x20), 2 },
        [TVBTN_CH_DOWN]  = { RC6(0x21), 2 },
        [TVBTN_0]        = { RC6(0x00), 0 },
        [TVBTN_1]        = { RC6(0x01), 0 },
        [TVBTN_2]        = { RC6(0x02), 0 },
        [TVBTN_3]        = { RC6(0x03), 0 },
        [TVBTN_4]        = { RC6(0x04), 0 },
        [TVBTN_5]        = { RC6(0x05), 0 },
        [TVBTN_6]        = { RC6(0x06), 0 },
        [TVBTN_7]        = { RC6(0x07), 0 },
        [TVBTN_8]        = { RC6(0x08), 0 },
        [TVBTN_9]        = { RC6(0x09), 0 },
        [TVBTN_UP]       = { RC6(0x58), 0 },
        [TVBTN_DOWN]     = { RC6(0x59), 0 },
        [TVBTN_LEFT]     = { RC6(0x5A), 0 },
        [TVBTN_RIGHT]    = { RC6(0x5B), 0 },
        [TVBTN_OK]       = { RC6(0x5C), 0 },
        [TVBTN_BACK]     = { RC6(0x28), 0 },
        [TVBTN_HOME]     = { RC6(0x64), 0 },
        [TVBTN_MENU]     = { RC6(0x54), 0 },
        [TVBTN_GUIDE]    = { RC6(0xCC), 0 },
        [TVBTN_INFO]     = { RC6(0x0F), 0 },
        [TVBTN_RED]      = { RC6(0x6D), 0 },
        [TVBTN_GREEN]    = { RC6(0x6E), 0 },
        [TVBTN_YELLOW]   = { RC6(0x6F), 0 },
        [TVBTN_BLUE]     = { RC6(0x70), 0 },
        [TVBTN_PLAY]     = { RC6(0x35), 0 },
        [TVBTN_PAUSE]    = { RC6(0x30), 0 },
        [TVBTN_STOP]     = { RC6(0x31), 0 },
        [TVBTN_REWIND]   = { RC6(0x33), 0 },
        [TVBTN_FFWD]     = { RC6(0x34), 0 },
    },
    .layout = nullptr, .layout_count = 0,
    .theme = {
        {0x22,0x22,0x22,0xFF},{0x00,0x2B,0x8C,0xFF},  // Philips dark blue
        {0x00,0x51,0xFF,0xFF},{0x33,0x33,0x33,0xFF},{0xFF,0xFF,0xFF,0xFF}
    }
};
MODEL_REGISTER(PHILIPS_50PUS6501)
