// Sanyo DP50E44 (2015) – NEC protocol, addr 0xE0E0
#include "../../model_registry.h"
#define SAN(cmd) ((uint32_t)(0xE0E00000|(uint8_t)(cmd)<<8|(uint8_t)(~(cmd))))
static const TVRemoteModel SANYO_DP50E44={
    .brand="Sanyo",.model="DP50E44",.year=2015,.protocol=IRProtocol::NEC,
    .cec_vendor=0x000000,.cec_osd_name_prefix="SANYO",
    .ir={
        [TVBTN_POWER]={SAN(0x08),0},[TVBTN_INPUT]={SAN(0x38),0},[TVBTN_MUTE]={SAN(0x0F),0},
        [TVBTN_VOL_UP]={SAN(0x1A),2},[TVBTN_VOL_DOWN]={SAN(0x1E),2},[TVBTN_CH_UP]={SAN(0x1F),2},[TVBTN_CH_DOWN]={SAN(0x1B),2},
        [TVBTN_0]={SAN(0x00),0},[TVBTN_1]={SAN(0x01),0},[TVBTN_2]={SAN(0x02),0},[TVBTN_3]={SAN(0x03),0},
        [TVBTN_4]={SAN(0x04),0},[TVBTN_5]={SAN(0x05),0},[TVBTN_6]={SAN(0x06),0},[TVBTN_7]={SAN(0x07),0},
        [TVBTN_8]={SAN(0x08),0},[TVBTN_9]={SAN(0x09),0},
        [TVBTN_UP]={SAN(0x58),0},[TVBTN_DOWN]={SAN(0x59),0},[TVBTN_LEFT]={SAN(0x5C),0},[TVBTN_RIGHT]={SAN(0x5B),0},
        [TVBTN_OK]={SAN(0x5A),0},[TVBTN_BACK]={SAN(0x28),0},[TVBTN_HOME]={0,0},[TVBTN_MENU]={SAN(0x54),0},
        [TVBTN_GUIDE]={SAN(0xCC),0},[TVBTN_INFO]={SAN(0x0F),0},
        [TVBTN_RED]={0,0},[TVBTN_GREEN]={0,0},[TVBTN_YELLOW]={0,0},[TVBTN_BLUE]={0,0},
        [TVBTN_PLAY]={SAN(0x35),0},[TVBTN_PAUSE]={SAN(0x30),0},[TVBTN_STOP]={SAN(0x31),0},[TVBTN_REWIND]={SAN(0x33),0},[TVBTN_FFWD]={SAN(0x34),0},
    },
    .layout=nullptr,.layout_count=0,
    .theme={{0x20,0x20,0x20,0xFF},{0xC8,0x00,0x00,0xFF},{0xFF,0x20,0x20,0xFF},{0x30,0x30,0x30,0xFF},{0xFF,0xFF,0xFF,0xFF}}
};
MODEL_REGISTER(SANYO_DP50E44)
