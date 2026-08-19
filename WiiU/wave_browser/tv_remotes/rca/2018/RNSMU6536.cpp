// Rca RNSMU6536 (2018) – NEC protocol
#include "../../model_registry.h"
#define RCA(cmd) ((uint32_t)(0xEE110000|(uint8_t)(cmd)<<8|(uint8_t)(~(cmd))))
static const TVRemoteModel RCA_RNSMU6536={
    .brand="Rca",.model="RNSMU6536",.year=2018,.protocol=IRProtocol::NEC,
    .cec_vendor=0x000000,.cec_osd_name_prefix="Rca",
    .ir={
        [TVBTN_POWER]={RCA(0x08),0},[TVBTN_INPUT]={RCA(0x38),0},[TVBTN_MUTE]={RCA(0x0F),0},
        [TVBTN_VOL_UP]={RCA(0x1A),2},[TVBTN_VOL_DOWN]={RCA(0x1E),2},
        [TVBTN_CH_UP]={RCA(0x1F),2},[TVBTN_CH_DOWN]={RCA(0x1B),2},
        [TVBTN_0]={RCA(0x00),0},[TVBTN_1]={RCA(0x01),0},[TVBTN_2]={RCA(0x02),0},
        [TVBTN_3]={RCA(0x03),0},[TVBTN_4]={RCA(0x04),0},[TVBTN_5]={RCA(0x05),0},
        [TVBTN_6]={RCA(0x06),0},[TVBTN_7]={RCA(0x07),0},[TVBTN_8]={RCA(0x08),0},[TVBTN_9]={RCA(0x09),0},
        [TVBTN_UP]={RCA(0x58),0},[TVBTN_DOWN]={RCA(0x59),0},
        [TVBTN_LEFT]={RCA(0x5C),0},[TVBTN_RIGHT]={RCA(0x5B),0},
        [TVBTN_OK]={RCA(0x5A),0},[TVBTN_BACK]={RCA(0x28),0},
        [TVBTN_HOME]={RCA(0x64),0},[TVBTN_MENU]={RCA(0x54),0},
        [TVBTN_GUIDE]={RCA(0xCC),0},[TVBTN_INFO]={RCA(0x0F),0},
        [TVBTN_RED]={RCA(0x6D),0},[TVBTN_GREEN]={RCA(0x6E),0},
        [TVBTN_YELLOW]={RCA(0x6F),0},[TVBTN_BLUE]={RCA(0x70),0},
        [TVBTN_PLAY]={RCA(0x35),0},[TVBTN_PAUSE]={RCA(0x30),0},
        [TVBTN_STOP]={RCA(0x31),0},[TVBTN_REWIND]={RCA(0x33),0},[TVBTN_FFWD]={RCA(0x34),0},
    },
    .layout=nullptr,.layout_count=0,
    .theme={{0x1A,0x1A,0x1A,0xFF},{0xC8,0x00,0x00,0xFF},{0xFF,0x20,0x20,0xFF},{0x2A,0x2A,0x2A,0xFF},{0xFF,0xFF,0xFF,0xFF}}
};
MODEL_REGISTER(RCA_RNSMU6536)
