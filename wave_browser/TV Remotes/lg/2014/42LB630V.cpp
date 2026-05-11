// LG 42LB630V (2014) – NEC protocol, address 0x20DF
// CEC Vendor ID: 0x00E091 (LG Electronics)

#include "../../model_registry.h"

static const TVRemoteModel LG_42LB630V = {
    .brand = "LG", .model = "42LB630V", .year = 2014,
    .protocol = IRProtocol::NEC,
    .cec_vendor = 0x00E091,
    .cec_osd_name_prefix = "LG",
    .ir = {
        [TVBTN_POWER]    = { 0x20DF10EF, 0 },
        [TVBTN_INPUT]    = { 0x20DFD02F, 0 },
        [TVBTN_MUTE]     = { 0x20DF906F, 0 },
        [TVBTN_VOL_UP]   = { 0x20DF40BF, 2 },
        [TVBTN_VOL_DOWN] = { 0x20DFC03F, 2 },
        [TVBTN_CH_UP]    = { 0x20DF00FF, 2 },
        [TVBTN_CH_DOWN]  = { 0x20DF807F, 2 },
        [TVBTN_0]        = { 0x20DF08F7, 0 },
        [TVBTN_1]        = { 0x20DF8877, 0 },
        [TVBTN_2]        = { 0x20DF48B7, 0 },
        [TVBTN_3]        = { 0x20DFC837, 0 },
        [TVBTN_4]        = { 0x20DF28D7, 0 },
        [TVBTN_5]        = { 0x20DFA857, 0 },
        [TVBTN_6]        = { 0x20DF6897, 0 },
        [TVBTN_7]        = { 0x20DFE817, 0 },
        [TVBTN_8]        = { 0x20DF18E7, 0 },
        [TVBTN_9]        = { 0x20DF9867, 0 },
        [TVBTN_UP]       = { 0x20DF02FD, 0 },
        [TVBTN_DOWN]     = { 0x20DF827D, 0 },
        [TVBTN_LEFT]     = { 0x20DFE01F, 0 },
        [TVBTN_RIGHT]    = { 0x20DF609F, 0 },
        [TVBTN_OK]       = { 0x20DF22DD, 0 },
        [TVBTN_BACK]     = { 0x20DF14EB, 0 },
        [TVBTN_HOME]     = { 0x20DFA15E, 0 },
        [TVBTN_MENU]     = { 0x20DFC23D, 0 },
        [TVBTN_GUIDE]    = { 0x20DF5EA1, 0 },
        [TVBTN_INFO]     = { 0x20DF55AA, 0 },
        [TVBTN_RED]      = { 0x20DF4EB1, 0 },
        [TVBTN_GREEN]    = { 0x20DF8E71, 0 },
        [TVBTN_YELLOW]   = { 0x20DFC6B9, 0 },
        [TVBTN_BLUE]     = { 0x20DF06F9, 0 },
        [TVBTN_PLAY]     = { 0x20DFB44B, 0 },
        [TVBTN_PAUSE]    = { 0x20DF7C83, 0 },
        [TVBTN_STOP]     = { 0x20DF3CC3, 0 },
        [TVBTN_REWIND]   = { 0x20DF5CA3, 0 },
        [TVBTN_FFWD]     = { 0x20DF1CE3, 0 },
    },
    .layout = nullptr, .layout_count = 0,
    .theme = {
        {0x1A,0x1A,0x2A,0xFF},{0xA5,0x00,0x64,0xFF},  // LG magenta
        {0xFF,0x00,0x80,0xFF},{0x2E,0x2E,0x3E,0xFF},{0xFF,0xFF,0xFF,0xFF}
    }
};
MODEL_REGISTER(LG_42LB630V)
