// Emerson LF551EM4 (2015) – NEC protocol
#include "../../model_registry.h"
#define EMR(cmd) ((uint32_t)(0x10EF0000|(uint8_t)(cmd)<<8|(uint8_t)(~(cmd))))
static const TVRemoteModel EMERSON_LF551EM4={
    .brand="Emerson",.model="LF551EM4",.year=2015,.protocol=IRProtocol::NEC,
    .cec_vendor=0x000000,.cec_osd_name_prefix="Emerson",
    .ir={
        [TVBTN_POWER]={EMR(0x08),0},[TVBTN_INPUT]={EMR(0x38),0},[TVBTN_MUTE]={EMR(0x0F),0},
        [TVBTN_VOL_UP]={EMR(0x1A),2},[TVBTN_VOL_DOWN]={EMR(0x1E),2},
        [TVBTN_CH_UP]={EMR(0x1F),2},[TVBTN_CH_DOWN]={EMR(0x1B),2},
        [TVBTN_0]={EMR(0x00),0},[TVBTN_1]={EMR(0x01),0},[TVBTN_2]={EMR(0x02),0},
        [TVBTN_3]={EMR(0x03),0},[TVBTN_4]={EMR(0x04),0},[TVBTN_5]={EMR(0x05),0},
        [TVBTN_6]={EMR(0x06),0},[TVBTN_7]={EMR(0x07),0},[TVBTN_8]={EMR(0x08),0},[TVBTN_9]={EMR(0x09),0},
        [TVBTN_UP]={EMR(0x58),0},[TVBTN_DOWN]={EMR(0x59),0},
        [TVBTN_LEFT]={EMR(0x5C),0},[TVBTN_RIGHT]={EMR(0x5B),0},
        [TVBTN_OK]={EMR(0x5A),0},[TVBTN_BACK]={EMR(0x28),0},
        [TVBTN_HOME]={EMR(0x64),0},[TVBTN_MENU]={EMR(0x54),0},
        [TVBTN_GUIDE]={EMR(0xCC),0},[TVBTN_INFO]={EMR(0x0F),0},
        [TVBTN_RED]={EMR(0x6D),0},[TVBTN_GREEN]={EMR(0x6E),0},
        [TVBTN_YELLOW]={EMR(0x6F),0},[TVBTN_BLUE]={EMR(0x70),0},
        [TVBTN_PLAY]={EMR(0x35),0},[TVBTN_PAUSE]={EMR(0x30),0},
        [TVBTN_STOP]={EMR(0x31),0},[TVBTN_REWIND]={EMR(0x33),0},[TVBTN_FFWD]={EMR(0x34),0},
    },
    .layout=nullptr,.layout_count=0,
    .theme={{0x1A,0x1A,0x1A,0xFF},{0x44,0x44,0x44,0xFF},{0x88,0x88,0x88,0xFF},{0x2A,0x2A,0x2A,0xFF},{0xFF,0xFF,0xFF,0xFF}}
};
MODEL_REGISTER(EMERSON_LF551EM4)
