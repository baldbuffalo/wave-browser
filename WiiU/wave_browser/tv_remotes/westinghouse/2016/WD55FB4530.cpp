// Westinghouse WD55FB4530 (2016) – NEC protocol
#include "../../model_registry.h"
#define WST(cmd) ((uint32_t)(0xFF000000|(uint8_t)(cmd)<<8|(uint8_t)(~(cmd))))
static const TVRemoteModel WESTINGHOUSE_WD55FB4530={
    .brand="Westinghouse",.model="WD55FB4530",.year=2016,.protocol=IRProtocol::NEC,
    .cec_vendor=0x000000,.cec_osd_name_prefix="Westinghouse",
    .ir={
        [TVBTN_POWER]={WST(0x08),0},[TVBTN_INPUT]={WST(0x38),0},[TVBTN_MUTE]={WST(0x0F),0},
        [TVBTN_VOL_UP]={WST(0x1A),2},[TVBTN_VOL_DOWN]={WST(0x1E),2},
        [TVBTN_CH_UP]={WST(0x1F),2},[TVBTN_CH_DOWN]={WST(0x1B),2},
        [TVBTN_0]={WST(0x00),0},[TVBTN_1]={WST(0x01),0},[TVBTN_2]={WST(0x02),0},
        [TVBTN_3]={WST(0x03),0},[TVBTN_4]={WST(0x04),0},[TVBTN_5]={WST(0x05),0},
        [TVBTN_6]={WST(0x06),0},[TVBTN_7]={WST(0x07),0},[TVBTN_8]={WST(0x08),0},[TVBTN_9]={WST(0x09),0},
        [TVBTN_UP]={WST(0x58),0},[TVBTN_DOWN]={WST(0x59),0},
        [TVBTN_LEFT]={WST(0x5C),0},[TVBTN_RIGHT]={WST(0x5B),0},
        [TVBTN_OK]={WST(0x5A),0},[TVBTN_BACK]={WST(0x28),0},
        [TVBTN_HOME]={WST(0x64),0},[TVBTN_MENU]={WST(0x54),0},
        [TVBTN_GUIDE]={WST(0xCC),0},[TVBTN_INFO]={WST(0x0F),0},
        [TVBTN_RED]={WST(0x6D),0},[TVBTN_GREEN]={WST(0x6E),0},
        [TVBTN_YELLOW]={WST(0x6F),0},[TVBTN_BLUE]={WST(0x70),0},
        [TVBTN_PLAY]={WST(0x35),0},[TVBTN_PAUSE]={WST(0x30),0},
        [TVBTN_STOP]={WST(0x31),0},[TVBTN_REWIND]={WST(0x33),0},[TVBTN_FFWD]={WST(0x34),0},
    },
    .layout=nullptr,.layout_count=0,
    .theme={{0x1A,0x1A,0x1A,0xFF},{0x00,0x6A,0x2E,0xFF},{0x00,0xA0,0x48,0xFF},{0x2A,0x2A,0x2A,0xFF},{0xFF,0xFF,0xFF,0xFF}}
};
MODEL_REGISTER(WESTINGHOUSE_WD55FB4530)
