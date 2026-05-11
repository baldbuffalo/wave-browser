// Vizio V505-G9 (2019) – V-Series 4K SmartCast

#include "../../model_registry.h"
#define VIZ(cmd) ((uint32_t)(0x20DF0000 | (uint8_t)(cmd) << 8 | (uint8_t)(~(cmd))))

static const TVRemoteModel VIZIO_V505G9 = {
    .brand = "Vizio", .model = "V505-G9", .year = 2019,
    .protocol = IRProtocol::NEC,
    .cec_vendor = 0x0E5E5E, .cec_osd_name_prefix = "VIZIO",
    .ir = {
        [TVBTN_POWER]    = { VIZ(0x09), 0 }, [TVBTN_INPUT]    = { VIZ(0x53), 0 },
        [TVBTN_MUTE]     = { VIZ(0x48), 0 }, [TVBTN_VOL_UP]   = { VIZ(0x40), 2 },
        [TVBTN_VOL_DOWN] = { VIZ(0x41), 2 }, [TVBTN_CH_UP]    = { VIZ(0x00), 2 },
        [TVBTN_CH_DOWN]  = { VIZ(0x01), 2 },
        [TVBTN_0] = {VIZ(0x10),0}, [TVBTN_1] = {VIZ(0x01),0},
        [TVBTN_2] = {VIZ(0x02),0}, [TVBTN_3] = {VIZ(0x03),0},
        [TVBTN_4] = {VIZ(0x04),0}, [TVBTN_5] = {VIZ(0x05),0},
        [TVBTN_6] = {VIZ(0x06),0}, [TVBTN_7] = {VIZ(0x07),0},
        [TVBTN_8] = {VIZ(0x08),0}, [TVBTN_9] = {VIZ(0x09),0},
        [TVBTN_UP]    = { VIZ(0x42), 0 }, [TVBTN_DOWN]  = { VIZ(0x43), 0 },
        [TVBTN_LEFT]  = { VIZ(0x44), 0 }, [TVBTN_RIGHT] = { VIZ(0x45), 0 },
        [TVBTN_OK]    = { VIZ(0x46), 0 }, [TVBTN_BACK]  = { VIZ(0x48), 0 },
        [TVBTN_HOME]  = { VIZ(0xBC), 0 }, [TVBTN_MENU]  = { VIZ(0x52), 0 },
        [TVBTN_GUIDE] = { VIZ(0x64), 0 }, [TVBTN_INFO]  = { VIZ(0x47), 0 },
        [TVBTN_RED]={0,0}, [TVBTN_GREEN]={0,0}, [TVBTN_YELLOW]={0,0}, [TVBTN_BLUE]={0,0},
        [TVBTN_PLAY]  = { VIZ(0x3B), 0 }, [TVBTN_PAUSE] = { VIZ(0x39), 0 },
        [TVBTN_STOP]  = { VIZ(0x38), 0 }, [TVBTN_REWIND]= { VIZ(0x37), 0 },
        [TVBTN_FFWD]  = { VIZ(0x36), 0 },
    },
    .layout = nullptr, .layout_count = 0,
    .theme = { {0x14,0x14,0x14,0xFF},{0x00,0x61,0xAA,0xFF},{0x00,0x90,0xFF,0xFF},{0x22,0x22,0x22,0xFF},{0xFF,0xFF,0xFF,0xFF} }
};
MODEL_REGISTER(VIZIO_V505G9)
