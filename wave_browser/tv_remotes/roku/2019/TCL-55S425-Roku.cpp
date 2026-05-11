// Roku TCL-55S425-Roku (2019) – NEC protocol
#include "../../model_registry.h"
#define ROK(cmd) ((uint32_t)(0xFF000000|(uint8_t)(cmd)<<8|(uint8_t)(~(cmd))))
static const TVRemoteModel ROKU_TCL_55S425_Roku={
    .brand="Roku",.model="TCL-55S425-Roku",.year=2019,.protocol=IRProtocol::NEC,
    .cec_vendor=0x000000,.cec_osd_name_prefix="Roku",
    .ir={
        [TVBTN_POWER]={ROK(0x08),0},[TVBTN_INPUT]={ROK(0x38),0},[TVBTN_MUTE]={ROK(0x0F),0},
        [TVBTN_VOL_UP]={ROK(0x1A),2},[TVBTN_VOL_DOWN]={ROK(0x1E),2},
        [TVBTN_CH_UP]={ROK(0x1F),2},[TVBTN_CH_DOWN]={ROK(0x1B),2},
        [TVBTN_0]={ROK(0x00),0},[TVBTN_1]={ROK(0x01),0},[TVBTN_2]={ROK(0x02),0},
        [TVBTN_3]={ROK(0x03),0},[TVBTN_4]={ROK(0x04),0},[TVBTN_5]={ROK(0x05),0},
        [TVBTN_6]={ROK(0x06),0},[TVBTN_7]={ROK(0x07),0},[TVBTN_8]={ROK(0x08),0},[TVBTN_9]={ROK(0x09),0},
        [TVBTN_UP]={ROK(0x58),0},[TVBTN_DOWN]={ROK(0x59),0},
        [TVBTN_LEFT]={ROK(0x5C),0},[TVBTN_RIGHT]={ROK(0x5B),0},
        [TVBTN_OK]={ROK(0x5A),0},[TVBTN_BACK]={ROK(0x28),0},
        [TVBTN_HOME]={ROK(0x64),0},[TVBTN_MENU]={ROK(0x54),0},
        [TVBTN_GUIDE]={ROK(0xCC),0},[TVBTN_INFO]={ROK(0x0F),0},
        [TVBTN_RED]={ROK(0x6D),0},[TVBTN_GREEN]={ROK(0x6E),0},
        [TVBTN_YELLOW]={ROK(0x6F),0},[TVBTN_BLUE]={ROK(0x70),0},
        [TVBTN_PLAY]={ROK(0x35),0},[TVBTN_PAUSE]={ROK(0x30),0},
        [TVBTN_STOP]={ROK(0x31),0},[TVBTN_REWIND]={ROK(0x33),0},[TVBTN_FFWD]={ROK(0x34),0},
    },
    .layout=nullptr,.layout_count=0,
    .theme={{0x1A,0x1A,0x1A,0xFF},{0x6E,0x00,0xF5,0xFF},{0xAA,0x44,0xFF,0xFF},{0x2A,0x2A,0x2A,0xFF},{0xFF,0xFF,0xFF,0xFF}}
};
MODEL_REGISTER(ROKU_TCL_55S425_Roku)
