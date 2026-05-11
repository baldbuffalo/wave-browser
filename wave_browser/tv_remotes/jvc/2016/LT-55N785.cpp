// JVC LT-55N785 (2016) – NEC protocol, addr 0xC5E8. CEC: 0x0054C3
#include "../../model_registry.h"
#define JVC_N(cmd) ((uint32_t)(0xC5E80000|(uint8_t)(cmd)<<8|(uint8_t)(~(cmd))))
static const TVRemoteModel JVC_LT55N785={
    .brand="JVC",.model="LT-55N785",.year=2016,.protocol=IRProtocol::NEC,
    .cec_vendor=0x0054C3,.cec_osd_name_prefix="JVC",
    .ir={
        [TVBTN_POWER]={JVC_N(0x08),0},[TVBTN_INPUT]={JVC_N(0x0B),0},[TVBTN_MUTE]={JVC_N(0x0F),0},
        [TVBTN_VOL_UP]={JVC_N(0x02),2},[TVBTN_VOL_DOWN]={JVC_N(0x03),2},[TVBTN_CH_UP]={JVC_N(0x00),2},[TVBTN_CH_DOWN]={JVC_N(0x01),2},
        [TVBTN_0]={JVC_N(0x10),0},[TVBTN_1]={JVC_N(0x11),0},[TVBTN_2]={JVC_N(0x12),0},[TVBTN_3]={JVC_N(0x13),0},
        [TVBTN_4]={JVC_N(0x14),0},[TVBTN_5]={JVC_N(0x15),0},[TVBTN_6]={JVC_N(0x16),0},[TVBTN_7]={JVC_N(0x17),0},
        [TVBTN_8]={JVC_N(0x18),0},[TVBTN_9]={JVC_N(0x19),0},
        [TVBTN_UP]={JVC_N(0x40),0},[TVBTN_DOWN]={JVC_N(0x41),0},[TVBTN_LEFT]={JVC_N(0x07),0},[TVBTN_RIGHT]={JVC_N(0x06),0},
        [TVBTN_OK]={JVC_N(0x09),0},[TVBTN_BACK]={JVC_N(0x0E),0},[TVBTN_HOME]={JVC_N(0x45),0},[TVBTN_MENU]={JVC_N(0x0A),0},
        [TVBTN_GUIDE]={JVC_N(0x4A),0},[TVBTN_INFO]={JVC_N(0x05),0},
        [TVBTN_RED]={JVC_N(0x72),0},[TVBTN_GREEN]={JVC_N(0x71),0},[TVBTN_YELLOW]={JVC_N(0x63),0},[TVBTN_BLUE]={JVC_N(0x61),0},
        [TVBTN_PLAY]={JVC_N(0x47),0},[TVBTN_PAUSE]={JVC_N(0x46),0},[TVBTN_STOP]={JVC_N(0x48),0},[TVBTN_REWIND]={JVC_N(0x49),0},[TVBTN_FFWD]={JVC_N(0x4B),0},
    },
    .layout=nullptr,.layout_count=0,
    .theme={{0x20,0x20,0x20,0xFF},{0x00,0x00,0x80,0xFF},{0x00,0x40,0xFF,0xFF},{0x30,0x30,0x30,0xFF},{0xFF,0xFF,0xFF,0xFF}}
};
MODEL_REGISTER(JVC_LT55N785)
