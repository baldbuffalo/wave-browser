// Samsung QE55Q7FN (2018) – Q-series QLED 4K Smart TV
// Q7 uses same IR codes but MENU→SETTINGS changed, and added AMBIENT mode.

#include "../../model_registry.h"

static const TVRemoteModel SAMSUNG_QE55Q7FN = {
    .brand = "Samsung", .model = "QE55Q7FN", .year = 2018,
    .protocol = IRProtocol::SAMSUNG,
    .cec_vendor = 0x0000F0, .cec_osd_name_prefix = "Samsung",
    .ir = {
        [TVBTN_POWER]    = { 0xE0E040BF, 0 },
        [TVBTN_INPUT]    = { 0xE0E0807F, 0 },
        [TVBTN_MUTE]     = { 0xE0E0F00F, 0 },
        [TVBTN_VOL_UP]   = { 0xE0E0E01F, 2 },
        [TVBTN_VOL_DOWN] = { 0xE0E0D02F, 2 },
        [TVBTN_CH_UP]    = { 0xE0E048B7, 2 },
        [TVBTN_CH_DOWN]  = { 0xE0E008F7, 2 },
        [TVBTN_0] = {0xE0E08877,0}, [TVBTN_1] = {0xE0E020DF,0},
        [TVBTN_2] = {0xE0E0A05F,0}, [TVBTN_3] = {0xE0E0609F,0},
        [TVBTN_4] = {0xE0E010EF,0}, [TVBTN_5] = {0xE0E0906F,0},
        [TVBTN_6] = {0xE0E050AF,0}, [TVBTN_7] = {0xE0E030CF,0},
        [TVBTN_8] = {0xE0E0B04F,0}, [TVBTN_9] = {0xE0E0708F,0},
        [TVBTN_UP]    = { 0xE0E006F9, 0 },
        [TVBTN_DOWN]  = { 0xE0E08679, 0 },
        [TVBTN_LEFT]  = { 0xE0E0A659, 0 },
        [TVBTN_RIGHT] = { 0xE0E046B9, 0 },
        [TVBTN_OK]    = { 0xE0E016E9, 0 },
        [TVBTN_BACK]  = { 0xE0E01AE5, 0 },
        [TVBTN_HOME]  = { 0xE0E0FC03, 0 },
        [TVBTN_MENU]  = { 0xE0E0C23D, 0 },   // Settings (Q-series)
        [TVBTN_GUIDE] = { 0xE0E0F20D, 0 },
        [TVBTN_INFO]  = { 0xE0E0F807, 0 },
        [TVBTN_RED]   = { 0xE0E036C9, 0 },
        [TVBTN_GREEN] = { 0xE0E028D7, 0 },
        [TVBTN_YELLOW]= { 0xE0E0A857, 0 },
        [TVBTN_BLUE]  = { 0xE0E06897, 0 },
        [TVBTN_PLAY]  = { 0xE0E0E21D, 0 },
        [TVBTN_PAUSE] = { 0xE0E052AD, 0 },
        [TVBTN_STOP]  = { 0xE0E0629D, 0 },
        [TVBTN_REWIND]= { 0xE0E0A25D, 0 },
        [TVBTN_FFWD]  = { 0xE0E012ED, 0 },
    },
    .layout = nullptr, .layout_count = 0,
    .theme = {
        {0x18,0x18,0x18,0xFF},{0x03,0x2C,0x6E,0xFF},
        {0x1A,0x73,0xE8,0xFF},{0x2A,0x2A,0x2A,0xFF},{0xFF,0xFF,0xFF,0xFF}
    }
};
MODEL_REGISTER(SAMSUNG_QE55Q7FN)
