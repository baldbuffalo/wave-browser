// Toshiba 55L621U (2015) – NEC protocol, address 0x02FD
// CEC Vendor ID: 0x000039 (Toshiba)
#include "../../model_registry.h"
#define TOS(cmd) ((uint32_t)(0x02FD0000 | (uint8_t)(cmd) << 8 | (uint8_t)(~(cmd))))
static const TVRemoteModel TOSHIBA_55L621U = {
    .brand = "Toshiba", .model = "55L621U", .year = 2015,
    .protocol = IRProtocol::NEC, .cec_vendor = 0x000039, .cec_osd_name_prefix = "Toshiba",
    .ir = {
        [TVBTN_POWER]    = { TOS(0x12), 0 }, [TVBTN_INPUT]    = { TOS(0x53), 0 },
        [TVBTN_MUTE]     = { TOS(0x15), 0 }, [TVBTN_VOL_UP]   = { TOS(0x1A), 2 },
        [TVBTN_VOL_DOWN] = { TOS(0x1E), 2 }, [TVBTN_CH_UP]    = { TOS(0x1F), 2 },
        [TVBTN_CH_DOWN]  = { TOS(0x1B), 2 },
        [TVBTN_0] = {TOS(0x00),0}, [TVBTN_1] = {TOS(0x01),0},
        [TVBTN_2] = {TOS(0x02),0}, [TVBTN_3] = {TOS(0x03),0},
        [TVBTN_4] = {TOS(0x04),0}, [TVBTN_5] = {TOS(0x05),0},
        [TVBTN_6] = {TOS(0x06),0}, [TVBTN_7] = {TOS(0x07),0},
        [TVBTN_8] = {TOS(0x08),0}, [TVBTN_9] = {TOS(0x09),0},
        [TVBTN_UP]    = { TOS(0x5C), 0 }, [TVBTN_DOWN]  = { TOS(0x5D), 0 },
        [TVBTN_LEFT]  = { TOS(0x5E), 0 }, [TVBTN_RIGHT] = { TOS(0x5F), 0 },
        [TVBTN_OK]    = { TOS(0x5B), 0 }, [TVBTN_BACK]  = { TOS(0x28), 0 },
        [TVBTN_HOME]  = { TOS(0x7D), 0 }, [TVBTN_MENU]  = { TOS(0x5A), 0 },
        [TVBTN_GUIDE] = { TOS(0x3D), 0 }, [TVBTN_INFO]  = { TOS(0x3F), 0 },
        [TVBTN_RED]   = { TOS(0x6D), 0 }, [TVBTN_GREEN] = { TOS(0x6E), 0 },
        [TVBTN_YELLOW]= { TOS(0x6F), 0 }, [TVBTN_BLUE]  = { TOS(0x70), 0 },
        [TVBTN_PLAY]  = { TOS(0x20), 0 }, [TVBTN_PAUSE] = { TOS(0x21), 0 },
        [TVBTN_STOP]  = { TOS(0x22), 0 }, [TVBTN_REWIND]= { TOS(0x23), 0 },
        [TVBTN_FFWD]  = { TOS(0x24), 0 },
    },
    .layout = nullptr, .layout_count = 0,
    .theme = { {0x1A,0x1A,0x22,0xFF},{0xCA,0x01,0x01,0xFF},{0xFF,0x30,0x30,0xFF},{0x2A,0x2A,0x32,0xFF},{0xFF,0xFF,0xFF,0xFF} }
};
MODEL_REGISTER(TOSHIBA_55L621U)
