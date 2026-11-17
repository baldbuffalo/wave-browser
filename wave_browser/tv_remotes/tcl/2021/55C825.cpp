// TCL 55C825 (2021) – Mini-LED QLED 4K Android TV

#include "../../model_registry.h"
#define TCL(cmd) ((uint32_t)(0xFF00 << 16 | (uint8_t)(cmd) << 8 | (uint8_t)(~(cmd))))

static const TVRemoteModel TCL_55C825 = {
    .brand = "TCL", .model = "55C825", .year = 2021,
    .protocol = IRProtocol::NEC,
    .cec_vendor = 0x00D7E2, .cec_osd_name_prefix = "TCL",
    .ir = {
        [TVBTN_POWER]    = { TCL(0x45), 0 }, [TVBTN_INPUT]    = { TCL(0x38), 0 },
        [TVBTN_MUTE]     = { TCL(0x48), 0 }, [TVBTN_VOL_UP]   = { TCL(0x46), 2 },
        [TVBTN_VOL_DOWN] = { TCL(0x15), 2 }, [TVBTN_CH_UP]    = { TCL(0x12), 2 },
        [TVBTN_CH_DOWN]  = { TCL(0x13), 2 },
        [TVBTN_0] = {TCL(0x19),0}, [TVBTN_1] = {TCL(0x16),0},
        [TVBTN_2] = {TCL(0x1A),0}, [TVBTN_3] = {TCL(0x1E),0},
        [TVBTN_4] = {TCL(0x11),0}, [TVBTN_5] = {TCL(0x0D),0},
        [TVBTN_6] = {TCL(0x0E),0}, [TVBTN_7] = {TCL(0x0F),0},
        [TVBTN_8] = {TCL(0x1F),0}, [TVBTN_9] = {TCL(0x1B),0},
        [TVBTN_UP]    = { TCL(0x52), 0 }, [TVBTN_DOWN]  = { TCL(0x53), 0 },
        [TVBTN_LEFT]  = { TCL(0x51), 0 }, [TVBTN_RIGHT] = { TCL(0x4F), 0 },
        [TVBTN_OK]    = { TCL(0x50), 0 }, [TVBTN_BACK]  = { TCL(0x1D), 0 },
        [TVBTN_HOME]  = { TCL(0x36), 0 }, [TVBTN_MENU]  = { TCL(0x40), 0 },
        [TVBTN_GUIDE] = { TCL(0xCC), 0 }, [TVBTN_INFO]  = { TCL(0x0B), 0 },
        [TVBTN_RED]   = { TCL(0x6D), 0 }, [TVBTN_GREEN] = { TCL(0x6E), 0 },
        [TVBTN_YELLOW]= { TCL(0x6F), 0 }, [TVBTN_BLUE]  = { TCL(0x70), 0 },
        [TVBTN_PLAY]  = { TCL(0x35), 0 }, [TVBTN_PAUSE] = { TCL(0x34), 0 },
        [TVBTN_STOP]  = { TCL(0x31), 0 }, [TVBTN_REWIND]= { TCL(0x21), 0 },
        [TVBTN_FFWD]  = { TCL(0x20), 0 },
    },
    .layout = nullptr, .layout_count = 0,
    .theme = { {0x14,0x14,0x14,0xFF},{0xC0,0x1C,0x22,0xFF},{0xFF,0x40,0x40,0xFF},{0x22,0x22,0x22,0xFF},{0xFF,0xFF,0xFF,0xFF} }
};
MODEL_REGISTER(TCL_55C825)
