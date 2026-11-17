// Hisense 55H9F (2018) – ULED 4K Android TV
#include "../../model_registry.h"
#define HIS(cmd) ((uint32_t)(0x4000 << 16 | (uint8_t)(cmd) << 8 | (uint8_t)(~(cmd))))

static const TVRemoteModel HISENSE_55H9F = {
    .brand = "Hisense", .model = "55H9F", .year = 2018,
    .protocol = IRProtocol::NEC, .cec_vendor = 0x00E739, .cec_osd_name_prefix = "Hisense",
    .ir = {
        [TVBTN_POWER]    = { HIS(0x08), 0 }, [TVBTN_INPUT]    = { HIS(0x0B), 0 },
        [TVBTN_MUTE]     = { HIS(0x0F), 0 }, [TVBTN_VOL_UP]   = { HIS(0x02), 2 },
        [TVBTN_VOL_DOWN] = { HIS(0x03), 2 }, [TVBTN_CH_UP]    = { HIS(0x00), 2 },
        [TVBTN_CH_DOWN]  = { HIS(0x01), 2 },
        [TVBTN_0] = {HIS(0x10),0}, [TVBTN_1] = {HIS(0x11),0}, [TVBTN_2] = {HIS(0x12),0},
        [TVBTN_3] = {HIS(0x13),0}, [TVBTN_4] = {HIS(0x14),0}, [TVBTN_5] = {HIS(0x15),0},
        [TVBTN_6] = {HIS(0x16),0}, [TVBTN_7] = {HIS(0x17),0}, [TVBTN_8] = {HIS(0x18),0},
        [TVBTN_9] = {HIS(0x19),0},
        [TVBTN_UP]    = { HIS(0x40), 0 }, [TVBTN_DOWN]  = { HIS(0x41), 0 },
        [TVBTN_LEFT]  = { HIS(0x07), 0 }, [TVBTN_RIGHT] = { HIS(0x06), 0 },
        [TVBTN_OK]    = { HIS(0x09), 0 }, [TVBTN_BACK]  = { HIS(0x0E), 0 },
        [TVBTN_HOME]  = { HIS(0x45), 0 }, [TVBTN_MENU]  = { HIS(0x0A), 0 },
        [TVBTN_GUIDE] = { HIS(0x4A), 0 }, [TVBTN_INFO]  = { HIS(0x05), 0 },
        [TVBTN_RED]   = { HIS(0x72), 0 }, [TVBTN_GREEN] = { HIS(0x71), 0 },
        [TVBTN_YELLOW]= { HIS(0x63), 0 }, [TVBTN_BLUE]  = { HIS(0x61), 0 },
        [TVBTN_PLAY]  = { HIS(0x47), 0 }, [TVBTN_PAUSE] = { HIS(0x46), 0 },
        [TVBTN_STOP]  = { HIS(0x48), 0 }, [TVBTN_REWIND]= { HIS(0x49), 0 },
        [TVBTN_FFWD]  = { HIS(0x4B), 0 },
    },
    .layout = nullptr, .layout_count = 0,
    .theme = { {0x18,0x18,0x18,0xFF},{0xE8,0x61,0x00,0xFF},{0xFF,0x90,0x20,0xFF},{0x28,0x28,0x28,0xFF},{0xFF,0xFF,0xFF,0xFF} }
};
MODEL_REGISTER(HISENSE_55H9F)
