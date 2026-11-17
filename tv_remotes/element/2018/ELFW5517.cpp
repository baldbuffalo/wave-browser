// Element ELFW5517 (2018) – NEC protocol
#include "../../model_registry.h"
#define ELM(cmd) ((uint32_t)(0xFF000000|(uint8_t)(cmd)<<8|(uint8_t)(~(cmd))))
static const TVRemoteModel ELEMENT_ELFW5517={
    .brand="Element",.model="ELFW5517",.year=2018,.protocol=IRProtocol::NEC,
    .cec_vendor=0x000000,.cec_osd_name_prefix="Element",
    .ir={
        [TVBTN_POWER]={ELM(0x08),0},[TVBTN_INPUT]={ELM(0x38),0},[TVBTN_MUTE]={ELM(0x0F),0},
        [TVBTN_VOL_UP]={ELM(0x1A),2},[TVBTN_VOL_DOWN]={ELM(0x1E),2},
        [TVBTN_CH_UP]={ELM(0x1F),2},[TVBTN_CH_DOWN]={ELM(0x1B),2},
        [TVBTN_0]={ELM(0x00),0},[TVBTN_1]={ELM(0x01),0},[TVBTN_2]={ELM(0x02),0},
        [TVBTN_3]={ELM(0x03),0},[TVBTN_4]={ELM(0x04),0},[TVBTN_5]={ELM(0x05),0},
        [TVBTN_6]={ELM(0x06),0},[TVBTN_7]={ELM(0x07),0},[TVBTN_8]={ELM(0x08),0},[TVBTN_9]={ELM(0x09),0},
        [TVBTN_UP]={ELM(0x58),0},[TVBTN_DOWN]={ELM(0x59),0},
        [TVBTN_LEFT]={ELM(0x5C),0},[TVBTN_RIGHT]={ELM(0x5B),0},
        [TVBTN_OK]={ELM(0x5A),0},[TVBTN_BACK]={ELM(0x28),0},
        [TVBTN_HOME]={ELM(0x64),0},[TVBTN_MENU]={ELM(0x54),0},
        [TVBTN_GUIDE]={ELM(0xCC),0},[TVBTN_INFO]={ELM(0x0F),0},
        [TVBTN_RED]={ELM(0x6D),0},[TVBTN_GREEN]={ELM(0x6E),0},
        [TVBTN_YELLOW]={ELM(0x6F),0},[TVBTN_BLUE]={ELM(0x70),0},
        [TVBTN_PLAY]={ELM(0x35),0},[TVBTN_PAUSE]={ELM(0x30),0},
        [TVBTN_STOP]={ELM(0x31),0},[TVBTN_REWIND]={ELM(0x33),0},[TVBTN_FFWD]={ELM(0x34),0},
    },
    .layout=nullptr,.layout_count=0,
    .theme={{0x1A,0x1A,0x1A,0xFF},{0x00,0x5A,0xA0,0xFF},{0x00,0x88,0xFF,0xFF},{0x2A,0x2A,0x2A,0xFF},{0xFF,0xFF,0xFF,0xFF}}
};
MODEL_REGISTER(ELEMENT_ELFW5517)
