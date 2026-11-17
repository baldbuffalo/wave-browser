// Insignia NS-55DF710NA19 (2018) – NEC protocol
#include "../../model_registry.h"
#define INS(cmd) ((uint32_t)(0x20DF0000|(uint8_t)(cmd)<<8|(uint8_t)(~(cmd))))
static const TVRemoteModel INSIGNIA_NS_55DF710NA19={
    .brand="Insignia",.model="NS-55DF710NA19",.year=2018,.protocol=IRProtocol::NEC,
    .cec_vendor=0x000000,.cec_osd_name_prefix="Insignia",
    .ir={
        [TVBTN_POWER]={INS(0x08),0},[TVBTN_INPUT]={INS(0x38),0},[TVBTN_MUTE]={INS(0x0F),0},
        [TVBTN_VOL_UP]={INS(0x1A),2},[TVBTN_VOL_DOWN]={INS(0x1E),2},
        [TVBTN_CH_UP]={INS(0x1F),2},[TVBTN_CH_DOWN]={INS(0x1B),2},
        [TVBTN_0]={INS(0x00),0},[TVBTN_1]={INS(0x01),0},[TVBTN_2]={INS(0x02),0},
        [TVBTN_3]={INS(0x03),0},[TVBTN_4]={INS(0x04),0},[TVBTN_5]={INS(0x05),0},
        [TVBTN_6]={INS(0x06),0},[TVBTN_7]={INS(0x07),0},[TVBTN_8]={INS(0x08),0},[TVBTN_9]={INS(0x09),0},
        [TVBTN_UP]={INS(0x58),0},[TVBTN_DOWN]={INS(0x59),0},
        [TVBTN_LEFT]={INS(0x5C),0},[TVBTN_RIGHT]={INS(0x5B),0},
        [TVBTN_OK]={INS(0x5A),0},[TVBTN_BACK]={INS(0x28),0},
        [TVBTN_HOME]={INS(0x64),0},[TVBTN_MENU]={INS(0x54),0},
        [TVBTN_GUIDE]={INS(0xCC),0},[TVBTN_INFO]={INS(0x0F),0},
        [TVBTN_RED]={INS(0x6D),0},[TVBTN_GREEN]={INS(0x6E),0},
        [TVBTN_YELLOW]={INS(0x6F),0},[TVBTN_BLUE]={INS(0x70),0},
        [TVBTN_PLAY]={INS(0x35),0},[TVBTN_PAUSE]={INS(0x30),0},
        [TVBTN_STOP]={INS(0x31),0},[TVBTN_REWIND]={INS(0x33),0},[TVBTN_FFWD]={INS(0x34),0},
    },
    .layout=nullptr,.layout_count=0,
    .theme={{0x1A,0x1A,0x1A,0xFF},{0x22,0x22,0x88,0xFF},{0x44,0x44,0xFF,0xFF},{0x2A,0x2A,0x2A,0xFF},{0xFF,0xFF,0xFF,0xFF}}
};
MODEL_REGISTER(INSIGNIA_NS_55DF710NA19)
