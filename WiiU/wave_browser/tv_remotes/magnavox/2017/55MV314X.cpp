// Magnavox 55MV314X (2017) – NEC protocol
#include "../../model_registry.h"
#define MAG(cmd) ((uint32_t)(0x17E80000|(uint8_t)(cmd)<<8|(uint8_t)(~(cmd))))
static const TVRemoteModel MAGNAVOX_55MV314X={
    .brand="Magnavox",.model="55MV314X",.year=2017,.protocol=IRProtocol::NEC,
    .cec_vendor=0x000000,.cec_osd_name_prefix="Magnavox",
    .ir={
        [TVBTN_POWER]={MAG(0x08),0},[TVBTN_INPUT]={MAG(0x38),0},[TVBTN_MUTE]={MAG(0x0F),0},
        [TVBTN_VOL_UP]={MAG(0x1A),2},[TVBTN_VOL_DOWN]={MAG(0x1E),2},
        [TVBTN_CH_UP]={MAG(0x1F),2},[TVBTN_CH_DOWN]={MAG(0x1B),2},
        [TVBTN_0]={MAG(0x00),0},[TVBTN_1]={MAG(0x01),0},[TVBTN_2]={MAG(0x02),0},
        [TVBTN_3]={MAG(0x03),0},[TVBTN_4]={MAG(0x04),0},[TVBTN_5]={MAG(0x05),0},
        [TVBTN_6]={MAG(0x06),0},[TVBTN_7]={MAG(0x07),0},[TVBTN_8]={MAG(0x08),0},[TVBTN_9]={MAG(0x09),0},
        [TVBTN_UP]={MAG(0x58),0},[TVBTN_DOWN]={MAG(0x59),0},
        [TVBTN_LEFT]={MAG(0x5C),0},[TVBTN_RIGHT]={MAG(0x5B),0},
        [TVBTN_OK]={MAG(0x5A),0},[TVBTN_BACK]={MAG(0x28),0},
        [TVBTN_HOME]={MAG(0x64),0},[TVBTN_MENU]={MAG(0x54),0},
        [TVBTN_GUIDE]={MAG(0xCC),0},[TVBTN_INFO]={MAG(0x0F),0},
        [TVBTN_RED]={MAG(0x6D),0},[TVBTN_GREEN]={MAG(0x6E),0},
        [TVBTN_YELLOW]={MAG(0x6F),0},[TVBTN_BLUE]={MAG(0x70),0},
        [TVBTN_PLAY]={MAG(0x35),0},[TVBTN_PAUSE]={MAG(0x30),0},
        [TVBTN_STOP]={MAG(0x31),0},[TVBTN_REWIND]={MAG(0x33),0},[TVBTN_FFWD]={MAG(0x34),0},
    },
    .layout=nullptr,.layout_count=0,
    .theme={{0x1A,0x1A,0x1A,0xFF},{0x22,0x55,0x99,0xFF},{0x44,0x88,0xFF,0xFF},{0x2A,0x2A,0x2A,0xFF},{0xFF,0xFF,0xFF,0xFF}}
};
MODEL_REGISTER(MAGNAVOX_55MV314X)
