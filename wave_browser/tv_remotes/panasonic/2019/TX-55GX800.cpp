// Panasonic TX-55GX800 (2019) – 4K UHD HDR, my Home Screen 4.0

#include "../../model_registry.h"
#define PAN(oem, cmd) ((uint32_t)(((uint32_t)(oem)<<16)|(cmd)))

static const TVRemoteModel PANASONIC_TX55GX800 = {
    .brand = "Panasonic", .model = "TX-55GX800", .year = 2019,
    .protocol = IRProtocol::PANASONIC,
    .cec_vendor = 0x008045, .cec_osd_name_prefix = "Panasonic",
    .ir = {
        [TVBTN_POWER]    = { PAN(0x0100,0x3D8C), 0 },
        [TVBTN_INPUT]    = { PAN(0x0100,0x3808), 0 },
        [TVBTN_MUTE]     = { PAN(0x0100,0x4C0C), 0 },
        [TVBTN_VOL_UP]   = { PAN(0x0100,0x4000), 2 },
        [TVBTN_VOL_DOWN] = { PAN(0x0100,0x4001), 2 },
        [TVBTN_CH_UP]    = { PAN(0x0100,0x4002), 2 },
        [TVBTN_CH_DOWN]  = { PAN(0x0100,0x4003), 2 },
        [TVBTN_0] = {PAN(0x0100,0x0080),0}, [TVBTN_1] = {PAN(0x0100,0x0081),0},
        [TVBTN_2] = {PAN(0x0100,0x0082),0}, [TVBTN_3] = {PAN(0x0100,0x0083),0},
        [TVBTN_4] = {PAN(0x0100,0x0084),0}, [TVBTN_5] = {PAN(0x0100,0x0085),0},
        [TVBTN_6] = {PAN(0x0100,0x0086),0}, [TVBTN_7] = {PAN(0x0100,0x0087),0},
        [TVBTN_8] = {PAN(0x0100,0x0088),0}, [TVBTN_9] = {PAN(0x0100,0x0089),0},
        [TVBTN_UP]    = { PAN(0x0100,0x4049), 0 }, [TVBTN_DOWN]  = { PAN(0x0100,0x404A), 0 },
        [TVBTN_LEFT]  = { PAN(0x0100,0x404B), 0 }, [TVBTN_RIGHT] = { PAN(0x0100,0x404C), 0 },
        [TVBTN_OK]    = { PAN(0x0100,0x4034), 0 }, [TVBTN_BACK]  = { PAN(0x0100,0x0D20), 0 },
        [TVBTN_HOME]  = { PAN(0x0100,0x4045), 0 }, [TVBTN_MENU]  = { PAN(0x0100,0x4046), 0 },
        [TVBTN_GUIDE] = { PAN(0x0100,0x0DCC), 0 }, [TVBTN_INFO]  = { PAN(0x0100,0x4048), 0 },
        [TVBTN_RED]   = { PAN(0x0100,0x0D00), 0 }, [TVBTN_GREEN] = { PAN(0x0100,0x0D01), 0 },
        [TVBTN_YELLOW]= { PAN(0x0100,0x0D02), 0 }, [TVBTN_BLUE]  = { PAN(0x0100,0x0D03), 0 },
        [TVBTN_PLAY]  = { PAN(0x0100,0x0D20), 0 }, [TVBTN_PAUSE] = { PAN(0x0100,0x0D21), 0 },
        [TVBTN_STOP]  = { PAN(0x0100,0x0D22), 0 }, [TVBTN_REWIND]= { PAN(0x0100,0x0D23), 0 },
        [TVBTN_FFWD]  = { PAN(0x0100,0x0D24), 0 },
    },
    .layout = nullptr, .layout_count = 0,
    .theme = { {0x1A,0x1A,0x28,0xFF},{0x00,0x47,0xBB,0xFF},{0x00,0x70,0xFF,0xFF},{0x2A,0x2A,0x38,0xFF},{0xFF,0xFF,0xFF,0xFF} }
};
MODEL_REGISTER(PANASONIC_TX55GX800)
