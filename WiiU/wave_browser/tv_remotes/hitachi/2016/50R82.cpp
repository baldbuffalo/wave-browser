// Hitachi 50R82 (2016) – NEC protocol, addr 0x0E0E. CEC: 0x00A000
#include "../../model_registry.h"
#define HIT(cmd) ((uint32_t)(0x0E0E0000|(uint8_t)(cmd)<<8|(uint8_t)(~(cmd))))
static const TVRemoteModel HITACHI_50R82={
    .brand="Hitachi",.model="50R82",.year=2016,.protocol=IRProtocol::NEC,
    .cec_vendor=0x00A000,.cec_osd_name_prefix="Hitachi",
    .ir={
        [TVBTN_POWER]={HIT(0x08),0},[TVBTN_INPUT]={HIT(0x0F),0},[TVBTN_MUTE]={HIT(0x0C),0},
        [TVBTN_VOL_UP]={HIT(0x1A),2},[TVBTN_VOL_DOWN]={HIT(0x1E),2},[TVBTN_CH_UP]={HIT(0x1F),2},[TVBTN_CH_DOWN]={HIT(0x1B),2},
        [TVBTN_0]={HIT(0x00),0},[TVBTN_1]={HIT(0x01),0},[TVBTN_2]={HIT(0x02),0},[TVBTN_3]={HIT(0x03),0},
        [TVBTN_4]={HIT(0x04),0},[TVBTN_5]={HIT(0x05),0},[TVBTN_6]={HIT(0x06),0},[TVBTN_7]={HIT(0x07),0},
        [TVBTN_8]={HIT(0x08),0},[TVBTN_9]={HIT(0x09),0},
        [TVBTN_UP]={HIT(0x58),0},[TVBTN_DOWN]={HIT(0x59),0},[TVBTN_LEFT]={HIT(0x5C),0},[TVBTN_RIGHT]={HIT(0x5B),0},
        [TVBTN_OK]={HIT(0x5A),0},[TVBTN_BACK]={HIT(0x28),0},[TVBTN_HOME]={HIT(0x60),0},[TVBTN_MENU]={HIT(0x40),0},
        [TVBTN_GUIDE]={HIT(0xCC),0},[TVBTN_INFO]={HIT(0x0B),0},
        [TVBTN_RED]={HIT(0x6D),0},[TVBTN_GREEN]={HIT(0x6E),0},[TVBTN_YELLOW]={HIT(0x6F),0},[TVBTN_BLUE]={HIT(0x70),0},
        [TVBTN_PLAY]={HIT(0x35),0},[TVBTN_PAUSE]={HIT(0x34),0},[TVBTN_STOP]={HIT(0x31),0},[TVBTN_REWIND]={HIT(0x21),0},[TVBTN_FFWD]={HIT(0x20),0},
    },
    .layout=nullptr,.layout_count=0,
    .theme={{0x18,0x18,0x18,0xFF},{0x8B,0x00,0x00,0xFF},{0xCC,0x00,0x00,0xFF},{0x28,0x28,0x28,0xFF},{0xFF,0xFF,0xFF,0xFF}}
};
MODEL_REGISTER(HITACHI_50R82)
